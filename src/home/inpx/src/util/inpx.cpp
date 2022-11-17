#pragma warning(push, 0)

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <numeric>

#include <plog/Log.h>

#include "fmt/core.h"
#include "sqlite/shell/sqlite_shell.h"
#include "sqlite3ppext.h"
#include "ZipLib/ZipFile.h"

#pragma warning(pop)

#include <set>

#include "constant.h"
#include "types.h"

#include "inpx.h"

#include "Configuration.h"

namespace {

int g_mhlTriggersOn = 1;

size_t g_id = 0;
size_t GetId()
{
	return ++g_id;
}

void Log(char * str)
{
	PLOGI << str;
}

class Timer
{
public:
	explicit Timer(std::wstring process_)
		: t(std::chrono::high_resolution_clock::now())
		, process(std::move(process_))
	{
		PLOGI << process << " started";
	}
	~Timer()
	{
		PLOGI << process << " done for " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - t).count() << " ms";
	}

	// rule 5
	Timer(const Timer &) = delete;
	Timer(Timer &&) = delete;
	Timer & operator=(const Timer &) = delete;
	Timer & operator=(Timer &&) = delete;

private:
	const std::chrono::steady_clock::time_point t;
	const std::wstring process;
};

size_t GetIdDefault(std::wstring_view)
{
	return GetId();
}

Dictionary::const_iterator FindDefault(const Dictionary & container, std::wstring_view value)
{
	return container.find(value);
}

bool IsComment(std::wstring_view line)
{
	return false
		|| std::size(line) < 3
		|| line.starts_with(COMMENT_START)
		;
}

class Ini
{
public:
	explicit Ini(std::map<std::wstring, std::wstring> data)
		: _data(std::move(data))
	{
	}

	explicit Ini(const std::filesystem::path & path)
	{
		if (!exists(path))
			throw std::invalid_argument("Need inpx file as command line argument");

		std::wstring line;

		std::wifstream iniStream(path);
		while (std::getline(iniStream, line))
		{
			if (IsComment(line))
				continue;

			auto it = std::cbegin(line);
			const auto key = Next(it, std::cend(line), INI_SEPARATOR);
			const auto value = Next(it, std::cend(line), INI_SEPARATOR);
			_data.emplace(key, value);
		}
	}

	const std::wstring & operator()(const wchar_t * key, const std::wstring & defaultValue) const
	{
		const auto it = _data.find(key);
		return it != _data.end() ? it->second : defaultValue;
	}

private:
	std::map<std::wstring, std::wstring> _data;
};

auto LoadGenres(std::wstring_view genresIniFileName)
{
	Genres genres;
	Dictionary index;

	std::ifstream iniStream(ToMultiByte(genresIniFileName));
	if (!iniStream.is_open())
		throw std::invalid_argument(fmt::format("Cannot open '{}'", ToMultiByte(genresIniFileName)));

	genres.emplace_back(L"0");
	index.emplace(genres.front().code, size_t { 0 });

	std::string buf;
	while (std::getline(iniStream, buf))
	{
		const auto line = ToWide(buf);
		if (IsComment(line))
			continue;

		auto it = std::cbegin(line);
		const auto codes = Next(it, std::cend(line), GENRE_SEPARATOR);
		auto itCode = std::cbegin(codes);
		std::wstring code;
		while (itCode != std::cend(codes))
		{
			const auto & added = index.emplace(Next(itCode, std::cend(codes), LIST_SEPARATOR), std::size(genres)).first->first;
			if (code.empty())
				code = added;
		}

		const auto parent = Next(it, std::cend(line), GENRE_SEPARATOR);
		const auto title  = Next(it, std::cend(line), GENRE_SEPARATOR);
		genres.emplace_back(code, parent, title);
	}

	std::for_each(std::next(std::begin(genres)), std::end(genres), [&index, &genres] (Genre & genre)
	{
		const auto it = index.find(genre.parentCore);
		assert(it != index.end());
		auto & parent = genres[it->second];
		genre.parentId = it->second;
		genre.dbCode = ToWide(fmt::format("{0}.{1}", ToMultiByte(parent.dbCode), ++parent.childrenCount));
	});

	return std::make_pair(std::move(genres), std::move(index));
}

template<typename T, T emptyValue = 0>
T Add(std::wstring_view value, Dictionary & container, const GetIdFunctor & getId = &GetIdDefault, const FindFunctor & find = &FindDefault)
{
	if (value.empty())
		return emptyValue;

	auto it = find(container, value);
	if (it == container.end())
		it = container.emplace(value, getId(value)).first;

	return static_cast<T>(it->second);
}

void ParseItem(size_t id, std::wstring_view data, Dictionary & container, Links & links, const GetIdFunctor & getId = &GetIdDefault, const FindFunctor & find = &FindDefault)
{
	auto it = std::cbegin(data);
	while (it != std::cend(data))
	{
		const auto value = Next(it, std::cend(data), LIST_SEPARATOR);
		links.emplace_back(id, Add<size_t>(value, container, getId, find));
	}
}

std::string & TrimRight(std::string & line)
{
	while (line.ends_with('\r'))
		line.pop_back();

	return line;
}

void ProcessCollectionInfo(std::istream & stream, SettingsTableData & settingsTableData)
{
	std::string line;
	for (size_t i = 0, sz = std::size(g_collectionInfoSettings); i < sz && std::getline(stream, line); ++i)
		if (g_collectionInfoSettings[i] != 0)
			settingsTableData[g_collectionInfoSettings[i]] = TrimRight(line);
}

void ProcessVersionInfo(std::istream & stream, SettingsTableData & settingsTableData)
{
	std::string line;
	if (std::getline(stream, line))
		settingsTableData[PROP_DATAVERSION] = TrimRight(line);
}

void ProcessInpx(std::istream & stream, const std::filesystem::path & rootFolder, std::wstring folder, Dictionary & genresIndex, Data & data, std::vector<std::wstring> & unknownGenres, size_t & n)
{
	const auto unknownGenreId = genresIndex.find(UNKNOWN)->second;
	auto & unknownGenre = data.genres[unknownGenreId];

	folder = std::filesystem::path(folder).replace_extension(ZIP).wstring();

	std::set<std::string> files;

	size_t insideNo = 0;
	std::string buf;
	while (std::getline(stream, buf))
	{
		const auto id = GetId();

		const auto line = ToWide(buf);
		auto it = std::cbegin(line);
		const auto end = std::cend(line);

		//AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;RATE;KEYWORDS;
		const auto authors = Next(it, end, FIELDS_SEPARATOR);
		const auto genres = Next(it, end, FIELDS_SEPARATOR);
		const auto title = Next(it, end, FIELDS_SEPARATOR);
		const auto seriesName = Next(it, end, FIELDS_SEPARATOR);
		const auto seriesNum = Next(it, end, FIELDS_SEPARATOR);
		const auto fileName = Next(it, end, FIELDS_SEPARATOR);
		const auto size = Next(it, end, FIELDS_SEPARATOR);
		const auto libId = Next(it, end, FIELDS_SEPARATOR);
		const auto del = Next(it, end, FIELDS_SEPARATOR);
		const auto ext = Next(it, end, FIELDS_SEPARATOR);
		const auto date = Next(it, end, FIELDS_SEPARATOR);
		const auto lang = Next(it, end, FIELDS_SEPARATOR);
		const auto rate = Next(it, end, FIELDS_SEPARATOR);
//		const auto keywords   = Next(it, end, FIELDS_SEPARATOR);

		files.emplace(ToMultiByte(fileName) + "." + ToMultiByte(ext));

		ParseItem(id, authors, data.authors, data.booksAuthors);
		ParseItem(id, genres, genresIndex, data.booksGenres,
			[unknownGenreId, &unknownGenre, &unknownGenres, &data = data.genres](std::wstring_view title)
			{
				const auto result = std::size(data);
				auto & genre = data.emplace_back(title, L"", title, unknownGenreId);
				genre.dbCode = ToWide(fmt::format("{0}.{1}", ToMultiByte(unknownGenre.dbCode), ++unknownGenre.childrenCount));
				unknownGenres.push_back(genre.name);
				return result;
			},
			[&data = data.genres](const Dictionary & container, std::wstring_view value)
			{
				const auto it = container.find(value);
				return it != container.end() ? it : std::ranges::find_if(container, [value, &data](const auto & item)
				{
					return IsStringEqual(value, data[item.second].name);
				});
			}
		);
		{
			const auto add = [&index = genresIndex, &genres = data.genres](std::wstring_view code, std::wstring_view name, const auto parentIt)
			{
				assert(parentIt != index.end() && parentIt->second < std::size(genres));
				const auto it = index.insert(std::make_pair(code, std::size(genres))).first;
				auto & genre = genres.emplace_back(code, genres[parentIt->second].code, name, parentIt->second);
				auto & parentGenre = genres[parentIt->second];
				genre.dbCode = ToWide(fmt::format("{0}.{1}", ToMultiByte(parentGenre.dbCode), ++parentGenre.childrenCount));
				return it;
			};

			auto itDate = std::cbegin(date);
			const auto endDate = std::cend(date);
			const auto year = Next(itDate, endDate, DATE_SEPARATOR);
			const auto month = Next(itDate, endDate, DATE_SEPARATOR);
			const auto dateCode = ToWide(fmt::format("date_{0}_{1}", ToMultiByte(year), ToMultiByte(month)));

			auto itIndexDate = genresIndex.find(dateCode);
			if (itIndexDate == genresIndex.end())
			{
				const auto yearCode = ToWide(fmt::format("year_{0}", ToMultiByte(year)));
				auto itIndexYear = genresIndex.find(yearCode);
				if (itIndexYear == genresIndex.end())
					itIndexYear = add(yearCode, year, genresIndex.find(DATE_ADDED_CODE));

				itIndexDate = add(dateCode, month, itIndexYear);
			}

			data.booksGenres.emplace_back(id, itIndexDate->second);
		}

		data.books.emplace_back(id, libId, title, Add<int, -1>(seriesName, data.series), To<int>(seriesNum, -1), date, To<int>(rate), lang, folder, fileName, insideNo++, ext, To<size_t>(size), To<bool>(del, false)/*, keywords*/);

		if ((++n % LOG_INTERVAL) == 0)
			PLOGI << n << " rows parsed";
	}

	const auto archive = ZipFile::Open(ToMultiByte(rootFolder.wstring() + L"/" + folder));
	const auto entriesCount = archive->GetEntriesCount();
	for (size_t i = 0; i < entriesCount; ++i)
	{
		const auto entry = archive->GetEntry(static_cast<int>(i));
		const auto fileName = entry->GetFullName();
		if (files.contains(fileName))
			continue;

		PLOGW << "Book is not indexed: " << ToMultiByte(folder) << "/" << fileName;
	}
}

struct InpxContent
{
	std::vector<std::wstring> collectionInfo;
	std::vector<std::wstring> versionInfo;
	std::vector<std::wstring> inpx;
};

InpxContent ExtractInpxFileNames(std::wstring_view inpxFileName)
{
	InpxContent inpxContent;

	const auto archive = ZipFile::Open(ToMultiByte(inpxFileName));
	const auto entriesCount = archive->GetEntriesCount();
	for (size_t i = 0; i < entriesCount; ++i)
	{
		const auto entry = archive->GetEntry(static_cast<int>(i));
		auto folder = ToWide(entry->GetFullName());

		if (folder == COLLECTION_INFO)
			inpxContent.collectionInfo.push_back(std::move(folder));
		else if (folder == VERSION_INFO)
			inpxContent.versionInfo.push_back(std::move(folder));
		else if (folder.ends_with(INP_EXT))
			inpxContent.inpx.push_back(std::move(folder));
		else
			PLOGI << folder << L" skipped";
	}

	return inpxContent;
}

std::pair<ZipArchiveEntry::Ptr, std::istream *> GetEntry(ZipArchive & archive, const std::wstring & name)
{
	PLOGI << name;
	auto entry = archive.GetEntry(ToMultiByte(name));
	auto * stream = entry->GetDecompressionStream();
	return std::make_pair(std::move(entry), stream);
}

Data Parse(std::wstring_view genresFileName, std::wstring_view inpxFileName, SettingsTableData && settingsTableData)
{
	Timer t(L"parsing archives");

	Data data;
	data.settings = std::move(settingsTableData);

	auto [genresData, genresIndex] = LoadGenres(genresFileName);
	data.genres = std::move(genresData);

	std::vector<std::wstring> unknownGenres;

	const auto inpxContent = ExtractInpxFileNames(inpxFileName);
	const auto archive = ZipFile::Open(ToMultiByte(inpxFileName));

	for (const auto & fileName : inpxContent.collectionInfo)
	{
		auto [_, stream] = GetEntry(*archive, fileName);
		ProcessCollectionInfo(*stream, data.settings);
	}

	for (const auto & fileName : inpxContent.versionInfo)
	{
		auto [_, stream] = GetEntry(*archive, fileName);
		ProcessVersionInfo(*stream, data.settings);
	}

	const auto rootFolder = std::filesystem::path(inpxFileName).parent_path();
	size_t n = 0;
	for (const auto & fileName : inpxContent.inpx)
	{
		auto [_, stream] = GetEntry(*archive, fileName);
		ProcessInpx(*stream, rootFolder, fileName, genresIndex, data, unknownGenres, n);
	}

	PLOGI << n << " rows parsed";

	if (!std::empty(unknownGenres))
	{
		PLOGW << "Unknown genres:";
		for (const auto & genre : unknownGenres)
			PLOGW << genre;
	}

	return data;
}

bool TableExists(sqlite3pp::database & db, const std::string & table)
{
	sqlite3pp::query query(db, fmt::format("SELECT name FROM sqlite_master WHERE type='table' AND name='{}'", table).data());
	return std::begin(query) != std::end(query);
}

SettingsTableData ReadSettings(const std::wstring & dbFileName)
{
	SettingsTableData data;

	sqlite3pp::database db(ToMultiByte(dbFileName).data(), SQLITE_OPEN_READONLY);
	if (!TableExists(db, "Settings"))
		return {};

	sqlite3pp::query query(db, "select SettingID, SettingValue from Settings");

	std::transform(std::begin(query), std::end(query), std::inserter(data, std::end(data)), [] (const auto & row)
	{
		int64_t id;
		const char * value;
		std::tie(id, value) = row.template get_columns<int64_t, char const *>(0, 1);
		return std::make_pair(static_cast<uint32_t>(id), std::string(value));
	});

	data[PROP_SCHEMA_VERSION] = SCHEMA_VERSION_VALUE;

	auto & strDate = data.emplace(PROP_CREATIONDATE, std::string()).first->second;
	strDate.resize(30);
	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::strftime(strDate.data(), strDate.size(), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

	return data;
}

void ExecuteScript(const std::wstring & action, std::wstring_view dbFileName, std::wstring_view scriptFileName)
{
	Timer t(action);

	char stubExe[] = "stub.exe";
	auto readScript = fmt::format(".read {}.", ToMultiByte(scriptFileName));
	auto loadExt = fmt::format(".load {}", MHL_SQLITE_EXTENSION);
	auto dbFileNameStr = ToMultiByte(dbFileName);

	char * v[]
	{
		stubExe,
		dbFileNameStr.data(),
		loadExt.data(),
		readScript.data(),
	};

	if (SQLiteShellExecute(static_cast<int>(std::size(v)), v, &Log) != 0)
		throw std::runtime_error(fmt::format("Cannot {}", ToMultiByte(action)));
}

template<typename It, typename Functor>
size_t StoreRange(std::wstring_view dbFileName, std::string_view process, std::string_view query, It beg, It end, Functor && f, bool addExt = true)
{
	const auto rowsTotal = static_cast<size_t>(std::distance(beg, end));
	Timer t(ToWide(fmt::format("store {0} {1}", process, rowsTotal)));
	size_t rowsInserted = 0;

	sqlite3pp::database db(ToMultiByte(dbFileName).data());
	std::unique_ptr<sqlite3pp::ext::function> func;
	if (addExt)
	{
		db.load_extension(MHL_SQLITE_EXTENSION);
		std::make_unique<sqlite3pp::ext::function>(db).swap(func);
		func->create("MHL_TRIGGERS_ON", [](sqlite3pp::ext::context & ctx) { ctx.result(g_mhlTriggersOn); });
	}

	sqlite3pp::transaction tr(db);
	sqlite3pp::command cmd(db, query.data());

	const auto log = [rowsTotal, &rowsInserted]
	{
		PLOGI << fmt::format("{0} rows inserted ({1}%)", rowsInserted, rowsInserted * 100 / rowsTotal);
	};

	const auto result = std::accumulate(beg, end, size_t { 0 }, [f = std::forward<Functor>(f), &db, &cmd, &rowsInserted, &log](size_t init, const typename It::value_type & value)
	{
		f(cmd, value);
		const auto result = 0
			+ cmd.execute()
			+ cmd.reset()
			;

		if (result == 0)
		{
			if (++rowsInserted % LOG_INTERVAL == 0)
				log();
		}
		else
		{
			PLOGE << db.error_code() << ": " << db.error_msg() << std::endl << value;
		}

		return init + result;
	});

	log();
	if (rowsTotal != rowsInserted)
		PLOGE << rowsTotal - rowsInserted << " rows lost";
	{
		Timer tc(L"commit");
		tr.commit();
	}

	return result;
}

size_t Store(std::wstring_view dbFileName, const Data & data)
{
	size_t result = 0;
	result += StoreRange(dbFileName, "Authors", "INSERT INTO Authors (AuthorID, LastName, FirstName, MiddleName) VALUES(?, ?, ?, ?)", std::cbegin(data.authors), std::cend(data.authors), [] (sqlite3pp::command & cmd, const Dictionary::value_type & item)
	{
		const auto & [author, id] = item;
		auto it = std::cbegin(author);
		const auto last   = Next(it, std::cend(author), NAMES_SEPARATOR);
		const auto first  = Next(it, std::cend(author), NAMES_SEPARATOR);
		const auto middle = Next(it, std::cend(author), NAMES_SEPARATOR);
		cmd.binder() << id << ToMultiByte(last) << ToMultiByte(first) << ToMultiByte(middle);
	});

	result += StoreRange(dbFileName, "Series", "INSERT INTO Series (SeriesID, SeriesTitle) VALUES (?, ?)", std::cbegin(data.series), std::cend(data.series), [] (sqlite3pp::command & cmd, const Dictionary::value_type & item)
	{
		cmd.binder() << item.second << ToMultiByte(item.first);
	});

	result += StoreRange(dbFileName, "Genres", "INSERT INTO Genres (GenreCode, ParentCode, FB2Code, GenreAlias) VALUES(?, ?, ?, ?)", std::next(std::cbegin(data.genres)), std::cend(data.genres), [&genres = data.genres](sqlite3pp::command & cmd, const Genre & genre)
	{
		cmd.binder() << ToMultiByte(genre.dbCode) << ToMultiByte(genres[genre.parentId].dbCode) << ToMultiByte(genre.code) << ToMultiByte(genre.name);
	});

	const char * queryText = "INSERT INTO Books ("
		"BookID   , LibID     , Title    , SeriesID, "
		"SeqNumber, UpdateDate, LibRate  , Lang    , "
		"Folder   , FileName  , InsideNo , Ext     , "
		"BookSize , IsLocal   , IsDeleted, KeyWords"
		") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
	result += StoreRange(dbFileName, "Books", queryText, std::cbegin(data.books), std::cend(data.books), [] (sqlite3pp::command & cmd, const Book & book)
	{
																		   cmd.bind( 1, book.id);
																		   cmd.bind( 2, ToMultiByte(book.libId), sqlite3pp::copy);
																		   cmd.bind( 3, ToMultiByte(book.title), sqlite3pp::copy);
			book.seriesId  == -1  ? cmd.bind( 4, sqlite3pp::null_type()) : cmd.bind( 4, book.seriesId);
			book.seriesNum == -1  ? cmd.bind( 5, sqlite3pp::null_type()) : cmd.bind( 5, book.seriesNum);
																		   cmd.bind( 6, ToMultiByte(book.date), sqlite3pp::copy);
																		   cmd.bind( 7, book.rate);
																		   cmd.bind( 8, ToMultiByte(book.language), sqlite3pp::copy);
																		   cmd.bind( 9, ToMultiByte(book.folder), sqlite3pp::copy);
																		   cmd.bind(10, ToMultiByte(book.fileName), sqlite3pp::copy);
																		   cmd.bind(11, book.insideNo);
																		   cmd.bind(12, ToMultiByte(book.format), sqlite3pp::copy);
																		   cmd.bind(13, book.size);
																		   cmd.bind(14, 1);
																		   cmd.bind(15, book.isDeleted ? 1 : 0);
			book.keywords.empty() ? cmd.bind(16, sqlite3pp::null_type()) : cmd.bind(16, ToMultiByte(book.keywords), sqlite3pp::copy);
	});

	result += StoreRange(dbFileName, "Author_List", "INSERT INTO Author_List (AuthorID, BookID) VALUES(?, ?)", std::cbegin(data.booksAuthors), std::cend(data.booksAuthors), [] (sqlite3pp::command & cmd, const Links::value_type & item)
	{
		cmd.binder() << item.second << item.first;
	}, false);

	result += StoreRange(dbFileName, "Genre_List", "INSERT INTO Genre_List (BookID, GenreCode) VALUES(?, ?)", std::cbegin(data.booksGenres), std::cend(data.booksGenres), [&genres = data.genres](sqlite3pp::command & cmd, const Links::value_type & item)
	{
		assert(item.second < std::size(genres));
		cmd.binder() << item.first << ToMultiByte(genres[item.second].dbCode);
	}, false);

	result += StoreRange(dbFileName, "Settings", "INSERT INTO Settings (SettingID, SettingValue) VALUES (?, ?)", std::cbegin(data.settings), std::cend(data.settings), [] (sqlite3pp::command & cmd, const SettingsTableData::value_type & item)
	{
		cmd.binder() << static_cast<size_t>(item.first) << item.second;
	}, false);

	return result;
}

bool Process(const Ini & ini)
{
	Timer t(L"work");

	g_mhlTriggersOn = _wtoi(ini(MHL_TRIGGERS_ON, std::to_wstring(g_mhlTriggersOn)).data());
	const auto dbFileName = ini(DB_PATH, DEFAULT_DB_PATH);

	auto settingsTableData = ReadSettings(dbFileName);
	ExecuteScript(L"create database", dbFileName, ini(DB_CREATE_SCRIPT, DEFAULT_DB_CREATE_SCRIPT));

	const auto data = Parse(ini(GENRES, DEFAULT_GENRES), ini(INPX, DEFAULT_INPX), std::move(settingsTableData));
	if (const auto failsCount = Store(dbFileName, data); failsCount != 0)
		PLOGE << "Something went wrong";

	ExecuteScript(L"update database", dbFileName, ini(DB_UPDATE_SCRIPT, DEFAULT_DB_UPDATE_SCRIPT));

	return true;
}

void RemoveExt(std::string & str)
{
	const auto dotPos = str.find_last_of('.');
	assert(dotPos != std::string::npos);
	str.resize(dotPos);
}

std::vector<std::string> GetNewInpxFolders(const Ini & ini)
{
	std::vector<std::string> result;
	std::set<std::string> dbFolders;

	{
		const auto dbFileName = ini(DB_PATH, DEFAULT_DB_PATH);
		sqlite3pp::database db(ToMultiByte(dbFileName).data(), SQLITE_OPEN_READONLY);
		db.load_extension(MHL_SQLITE_EXTENSION);
		if (!TableExists(db, "Books"))
			return result;

		sqlite3pp::query query(db, "select distinct Folder from Books");
		std::transform(std::begin(query), std::end(query), std::inserter(dbFolders, std::end(dbFolders)), [] (const auto & row)
		{
			auto folder = row.template get<std::string>(0);
			RemoveExt(folder);
			return folder;
		});
	}

	std::set<std::string> inpxFolders;
	std::ranges::transform(ExtractInpxFileNames(ini(INPX, DEFAULT_INPX)).inpx, std::inserter(inpxFolders, std::end(inpxFolders)), [] (const std::wstring & item)
	{
		auto folder = ToMultiByte(item);
		RemoveExt(folder);
		return folder;
	});

	std::ranges::set_difference(inpxFolders, dbFolders, std::back_inserter(result));

	return result;
}

template<typename T>
int ParseInpxImpl(T t)
{
	try
	{
		const Ini ini(std::forward<T>(t));
		return Process(ini);
	}
	catch (const std::exception & ex)
	{
		PLOGE << ex.what();
	}
	catch (...)
	{
		PLOGE << "unknown error";
	}
	return false;
}

}

namespace HomeCompa::Inpx {

bool CreateNewCollection(const std::filesystem::path & iniFile)
{
	return ParseInpxImpl(iniFile);
}

bool CreateNewCollection(std::map<std::wstring, std::wstring> data)
{
	return ParseInpxImpl(std::move(data));
}

bool CheckUpdateCollection(std::map<std::wstring, std::wstring> data)
{
	const Ini ini(std::move(data));
	return !GetNewInpxFolders(ini).empty();
}

bool UpdateCollection(std::map<std::wstring, std::wstring> data)
{
	return true;
}

}
