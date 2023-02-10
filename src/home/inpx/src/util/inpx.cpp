#pragma warning(push, 0)

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <numeric>
#include <set>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <plog/Log.h>

#include "fmt/core.h"
#include "sqlite3ppext.h"

#pragma warning(pop)

#include "constant.h"
#include "types.h"

#include "inpx.h"
#include "Configuration.h"

namespace {

size_t g_id = 0;
size_t GetId()
{
	return ++g_id;
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
	explicit Ini(std::map<std::wstring, std::filesystem::path> data)
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

	const std::filesystem::path & operator()(const wchar_t * key, const std::filesystem::path & defaultValue) const
	{
		const auto it = _data.find(key);
		return it != _data.end() ? it->second : defaultValue;
	}

private:
	std::map<std::wstring, std::filesystem::path> _data;
};

class DatabaseWrapper
{
public:
	explicit DatabaseWrapper(const std::filesystem::path & dbFileName, int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
		: m_db(dbFileName.generic_string().data(), flags)
		, m_func(m_db)
	{
		m_db.load_extension(MHL_SQLITE_EXTENSION);
		m_func.create("MHL_TRIGGERS_ON", [] (sqlite3pp::ext::context & ctx)
		{
			ctx.result(1);
		});
	}
public:
	operator sqlite3pp::database & ()
	{
		return m_db;
	}

	sqlite3pp::database * operator->()
	{
		return &m_db;
	}

private:
	sqlite3pp::database m_db;
	sqlite3pp::ext::function m_func;
};

auto LoadGenres(const std::filesystem::path & genresIniFileName)
{
	Genres genres;
	Dictionary index;

	std::ifstream iniStream(genresIniFileName);
	if (!iniStream.is_open())
		throw std::invalid_argument(fmt::format("Cannot open '{}'", genresIniFileName.generic_string()));

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
		genre.dbCode = ToWide(fmt::format("{:s}.{:03d}", ToMultiByte(parent.dbCode), ++parent.childrenCount));
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

void ProcessCollectionInfo(QIODevice & stream, SettingsTableData & settingsTableData)
{
	for (size_t i = 0, sz = std::size(g_collectionInfoSettings); i < sz; ++i)
	{
		if (g_collectionInfoSettings[i] != 0)
		{
			if (const auto bytes = stream.readLine(); !bytes.isEmpty())
			{
				auto line = bytes.toStdString();
				settingsTableData[g_collectionInfoSettings[i]] = TrimRight(line);
			}
		}
	}
}

void ProcessVersionInfo(QIODevice & stream, SettingsTableData & settingsTableData)
{
	if (const auto bytes = stream.readLine(); !bytes.isEmpty())
	{
		auto line = bytes.toStdString();
		settingsTableData[PROP_DATAVERSION] = TrimRight(line);
	}
}

void ProcessInpx(QIODevice & stream, const std::filesystem::path & rootFolder, std::wstring folder, Dictionary & genresIndex, Data & data, std::vector<std::wstring> & unknownGenres, size_t & n)
{
	const auto unknownGenreId = genresIndex.find(UNKNOWN)->second;
	auto & unknownGenre = data.genres[unknownGenreId];

	folder = std::filesystem::path(folder).replace_extension(ZIP).wstring();

	std::set<std::string> files;

	size_t insideNo = 0;

	while (true)
	{
		const auto byteArray = stream.readLine();
		if (byteArray.isEmpty())
			break;

		const auto id = GetId();

		const auto line = ToWide(byteArray.constData());
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

				itIndexDate = add(dateCode, std::wstring(year).append(L".").append(month), itIndexYear);
			}

			data.booksGenres.emplace_back(id, itIndexDate->second);
		}

		data.books.emplace_back(id, libId, title, Add<int, -1>(seriesName, data.series), To<int>(seriesNum, -1), date, To<int>(rate), lang, folder, fileName, insideNo++, ext, To<size_t>(size), To<bool>(del, false)/*, keywords*/);

		if ((++n % LOG_INTERVAL) == 0)
			PLOGI << n << " rows parsed";
	}

	QuaZip zip(QString::fromStdWString(rootFolder / folder));
	zip.open(QuaZip::mdUnzip);
	for (const auto & fileName : zip.getFileNameList())
	{
		if (files.contains(fileName.toStdString()))
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

InpxContent ExtractInpxFileNames(const std::filesystem::path & inpxFileName)
{
	InpxContent inpxContent;

	QuaZip zip(QString::fromStdWString(inpxFileName.generic_wstring()));

	std::ifstream zipStream(inpxFileName, std::ios::binary);
	zip.open(QuaZip::mdUnzip);
	for (const auto & fileName : zip.getFileNameList())
	{
		auto folder = ToWide(fileName.toStdString());

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

std::unique_ptr<QIODevice> GetDecodedStream(QuaZip & zip, const std::wstring & file)
{
	PLOGI << file;
	zip.setCurrentFile(QString::fromStdWString(file));
	auto zipFile = std::make_unique<QuaZipFile>(&zip);
	if (!zipFile->open(QIODevice::ReadOnly))
		throw std::runtime_error("Cannot open " + ToMultiByte(file));

	return zipFile;
}

void ParseInpxFiles(const std::filesystem::path & inpxFileName, QuaZip & zip, const std::vector<std::wstring> & inpxFiles, Dictionary & genresIndex, Data & data)
{
	std::vector<std::wstring> unknownGenres;

	const auto rootFolder = std::filesystem::path(inpxFileName).parent_path();
	size_t n = 0;
	for (const auto & fileName : inpxFiles)
		if (auto zipDecodedStream = GetDecodedStream(zip, fileName))
			ProcessInpx(*zipDecodedStream, rootFolder, fileName, genresIndex, data, unknownGenres, n);

	PLOGI << n << " rows parsed";

	if (!std::empty(unknownGenres))
	{
		PLOGW << "Unknown genres:";
		for (const auto & genre : unknownGenres)
			PLOGW << genre;
	}
}

Data Parse(const std::filesystem::path & genresFileName, const std::filesystem::path & inpxFileName, SettingsTableData && settingsTableData)
{
	Timer t(L"parsing archives");

	Data data;
	data.settings = std::move(settingsTableData);

	auto [genresData, genresIndex] = LoadGenres(genresFileName);
	data.genres = std::move(genresData);

	const auto inpxContent = ExtractInpxFileNames(inpxFileName);
	QuaZip zip(QString::fromStdWString(inpxFileName.generic_wstring()));
	zip.open(QuaZip::mdUnzip);

	for (const auto & fileName : inpxContent.collectionInfo)
		if (auto zipDecodedStream = GetDecodedStream(zip, fileName))
			ProcessCollectionInfo(*zipDecodedStream, data.settings);

	for (const auto & fileName : inpxContent.versionInfo)
		if (auto zipDecodedStream = GetDecodedStream(zip, fileName))
			ProcessVersionInfo(*zipDecodedStream, data.settings);

	ParseInpxFiles(inpxFileName, zip, inpxContent.inpx, genresIndex, data);

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

	if (!std::filesystem::exists(dbFileName))
		return data;

	DatabaseWrapper db(dbFileName, SQLITE_OPEN_READONLY);
	if (!TableExists(db, "Settings"))
		return data;

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

void ExecuteScript(const std::wstring & action, const std::filesystem::path & dbFileName, const std::filesystem::path & scriptFileName)
{
	Timer t(action);

	DatabaseWrapper db(dbFileName);

	std::string command;
	std::string str;
	str.resize(1024);
	std::ifstream inp(scriptFileName);
	if (!inp.is_open())
		throw std::runtime_error(fmt::format("Cannot open {}", scriptFileName.generic_string()));

	while(!inp.eof())
	{
		command.clear();
		while (!inp.eof())
		{
			inp.getline(str.data(), str.size());
			assert(inp.good() || inp.eof());
			if (str.starts_with("--@@"))
				break;

			if (str.front())
				command.append(str.data()).append("\n");
		}

		if (command.empty() || !command.front())
			continue;

		PLOGI << command;
		if (command.starts_with("PRAGMA"))
		{
			sqlite3pp::query(db, command.data()).begin();
		}
		else
		{
			[[maybe_unused]] const auto rc = sqlite3pp::command(db, command.data()).execute();
			assert(rc == 0);
		}
	}
}

template<typename It, typename Functor>
size_t StoreRange(const std::filesystem::path & dbFileName, std::string_view process, std::string_view query, It beg, It end, Functor && f)
{
	const auto rowsTotal = static_cast<size_t>(std::distance(beg, end));
	if (rowsTotal == 0)
		return rowsTotal;

	Timer t(ToWide(fmt::format("store {0} {1}", process, rowsTotal)));
	size_t rowsInserted = 0;

	DatabaseWrapper db(dbFileName);
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
			PLOGE << db->error_code() << ": " << db->error_msg() << std::endl << value;
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

size_t Store(const std::filesystem::path & dbFileName, const Data & data)
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

	std::vector<size_t> newGenresIndex;
	for (size_t i = 0, sz = std::size(data.genres); i < sz; ++i)
		if (data.genres[i].newGenre)
			newGenresIndex.push_back(i);
	result += StoreRange(dbFileName, "Genres", "INSERT INTO Genres (GenreCode, ParentCode, FB2Code, GenreAlias) VALUES(?, ?, ?, ?)", std::next(std::cbegin(newGenresIndex)), std::cend(newGenresIndex), [&genres = data.genres](sqlite3pp::command & cmd, const size_t n)
	{
		cmd.binder() << ToMultiByte(genres[n].dbCode) << ToMultiByte(genres[genres[n].parentId].dbCode) << ToMultiByte(genres[n].code) << ToMultiByte(genres[n].name);
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
	});

	result += StoreRange(dbFileName, "Genre_List", "INSERT INTO Genre_List (BookID, GenreCode) VALUES(?, ?)", std::cbegin(data.booksGenres), std::cend(data.booksGenres), [&genres = data.genres](sqlite3pp::command & cmd, const Links::value_type & item)
	{
		assert(item.second < std::size(genres));
		cmd.binder() << item.first << ToMultiByte(genres[item.second].dbCode);
	});

	result += StoreRange(dbFileName, "Settings", "INSERT INTO Settings (SettingID, SettingValue) VALUES (?, ?)", std::cbegin(data.settings), std::cend(data.settings), [] (sqlite3pp::command & cmd, const SettingsTableData::value_type & item)
	{
		cmd.binder() << static_cast<size_t>(item.first) << item.second;
	});

	return result;
}

bool Process(const Ini & ini)
{
	Timer t(L"work");

	const auto & dbFileName = ini(DB_PATH, DEFAULT_DB_PATH);

	auto settingsTableData = ReadSettings(dbFileName);
	ExecuteScript(L"create database", dbFileName, ini(DB_CREATE_SCRIPT, DEFAULT_DB_CREATE_SCRIPT));

	const auto data = Parse(ini(GENRES, DEFAULT_GENRES), ini(INPX, DEFAULT_INPX), std::move(settingsTableData));
	if (const auto failsCount = Store(dbFileName, data); failsCount != 0)
		PLOGE << "Something went wrong";

	ExecuteScript(L"update database", dbFileName, ini(DB_UPDATE_SCRIPT, DEFAULT_DB_UPDATE_SCRIPT));

	return true;
}

std::wstring RemoveExt(std::wstring & str)
{
	const auto dotPos = str.find_last_of(L'.');
	assert(dotPos != std::string::npos);
	std::wstring result(std::next(std::cbegin(str), dotPos + 1), std::cend(str));
	str.resize(dotPos);
	return result;
}

std::vector<std::wstring> GetNewInpxFolders(const Ini & ini)
{
	std::vector<std::wstring> result;
	std::set<std::wstring> dbFolders;

	{
		const auto dbFileName = ini(DB_PATH, DEFAULT_DB_PATH).generic_string();
		DatabaseWrapper db(dbFileName.data(), SQLITE_OPEN_READONLY);
		if (!TableExists(db, "Books"))
			return result;

		sqlite3pp::query query(db, "select distinct Folder from Books");
		std::transform(std::begin(query), std::end(query), std::inserter(dbFolders, std::end(dbFolders)), [] (const auto & row)
		{
			auto folder = ToWide(row.template get<std::string>(0));
			RemoveExt(folder);
			return folder;
		});
	}

	std::set<std::wstring> inpxFolders;
	std::map<std::wstring, std::wstring> extentions;
	std::ranges::transform(ExtractInpxFileNames(ini(INPX, DEFAULT_INPX)).inpx, std::inserter(inpxFolders, std::end(inpxFolders)), [&extentions] (std::wstring item)
	{
		auto ext = RemoveExt(item);
		extentions.emplace(item, std::move(ext));
		return item;
	});

	std::ranges::set_difference(inpxFolders, dbFolders, std::back_inserter(result));
	std::ranges::transform(result, std::begin(result), [&extentions] (const std::wstring & item)
	{
		const auto it = extentions.find(item);
		assert(it != extentions.end());
		return item + L'.' + it->second;
	});

	return result;
}

Dictionary ReadDictionary(std::string_view name, sqlite3pp::database & db, const char * statement)
{
	PLOGI << "Read " << name;
	Dictionary data;
	sqlite3pp::query query(db, statement);
	std::transform(std::begin(query), std::end(query), std::inserter(data, std::end(data)), [] (const auto & row)
	{
		int64_t id;
		const char * value;
		std::tie(id, value) = row.template get_columns<int64_t, char const *>(0, 1);
		return std::make_pair(ToWide(value), static_cast<size_t>(id));
	});
	return data;
}

std::pair<Genres, Dictionary> ReadGenres(sqlite3pp::database & db, const std::filesystem::path & genresFileName)
{
	PLOGI << "Read genres";
	std::pair<Genres, Dictionary> result;
	auto & [genres, index] = result;

	genres.emplace_back(L"0");
	index.emplace(genres.front().code, size_t { 0 });

	sqlite3pp::query query(db, "select FB2Code, GenreCode, ParentCode, GenreAlias from Genres");
	std::map<std::wstring, std::vector<size_t>> children;
	std::transform(std::begin(query), std::end(query), std::inserter(genres, std::end(genres)), [&, n = std::size(genres)](const auto & row) mutable
	{
		const auto fb2Code = row.template get<std::string>(0);
		const auto genreCode = row.template get<std::string>(1);
		const auto parentCode = row.template get<std::string>(2);
		const auto genreAlias = row.template get<std::string>(3);

		Genre genre(ToWide(fb2Code), L"", ToWide(genreAlias));
		genre.dbCode = ToWide(genreCode);
		children[ToWide(parentCode)].push_back(n);
		index.emplace(genre.code, n++);
		genre.newGenre = false;

		return genre;
	});

	for (size_t i = 0, sz = std::size(genres); i < sz; ++i)
	{
		const auto it = children.find(genres[i].dbCode);
		if (it == children.end())
			continue;

		genres[i].childrenCount = std::size(it->second);
		for (const auto n : it->second)
		{
			genres[n].parentId = i;
			genres[n].parentCore = genres[i].code;
		}
	}

	auto n = std::size(genres);

	auto [iniGenres, iniIndex] = LoadGenres(genresFileName);
	for (const auto & genre : iniGenres)
	{
		if (index.contains(genre.code))
			continue;

		index.emplace(genre.code, std::size(genres));
		genres.push_back(genre);
	}

	for (const auto sz = std::size(genres); n < sz; ++n)
	{
		const auto & parent = iniGenres[iniGenres[n].parentId];
		const auto it = index.find(parent.code);
		assert(it != index.end());
		iniGenres[n].parentId = it->second;
	}

	for (const auto & [code, k] : iniIndex)
	{
		if (index.contains(code))
			continue;

		const auto & genre = iniGenres[k];
		const auto it = index.find(genre.code);
		assert(it != index.end());
		index.emplace(code, it->second);
	}

	return result;
}

void SetNextId(sqlite3pp::database & db)
{
	sqlite3pp::query query(db,
		"select coalesce(max(m), 0) from "
		"(select max(BookID) m from Books "
		"union select max(AuthorID) m from Authors "
		"union select max(SeriesID) m from Series)");
	g_id = (*query.begin()).get<long long>(0);
	PLOGI << "Next Id: " << g_id;
}

std::pair<Data, Dictionary> ReadData(const std::filesystem::path & dbFileName, const std::filesystem::path & genresFileName)
{
	std::pair<Data, Dictionary> result;
	auto & [data, index] = result;

	DatabaseWrapper db(dbFileName, SQLITE_OPEN_READONLY);
	SetNextId(db);
	data.authors = ReadDictionary("authors", db, "select AuthorID, LastName||','||FirstName||','||MiddleName from Authors");
	data.series = ReadDictionary("series", db, "select SeriesID, SeriesTitle from Series");
	auto [genres, genresIndex] = ReadGenres(db, genresFileName);
	data.genres = std::move(genres);
	index = std::move(genresIndex);

	return result;
}

bool UpdateDatabase(const Ini & ini)
{
	const auto & dbFileName = ini(DB_PATH, DEFAULT_DB_PATH);
	const auto [oldData, oldGenresIndex] = ReadData(dbFileName, ini(GENRES, DEFAULT_GENRES));

	Data newData = oldData;
	Dictionary newGenresIndex = oldGenresIndex;
	auto inpxFileName = ini(INPX, DEFAULT_INPX);
	QuaZip zip(QString::fromStdWString(inpxFileName));
	zip.open(QuaZip::mdUnzip);
	ParseInpxFiles(inpxFileName, zip, GetNewInpxFolders(ini), newGenresIndex, newData);

	const auto filter = [] (Dictionary & dst, const Dictionary & src)
	{
		for (auto it = dst.begin(); it != dst.end(); )
			if (src.contains(it->first))
				it = dst.erase(it);
			else
				++it;
	};

	filter(newData.authors, oldData.authors);
	filter(newData.series, oldData.series);

	if (const auto failsCount = Store(dbFileName, newData); failsCount != 0)
		PLOGE << "Something went wrong";

	return true;
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

bool CreateNewCollection(std::map<std::wstring, std::filesystem::path> data)
{
	return ParseInpxImpl(std::move(data));
}

bool CheckUpdateCollection(std::map<std::wstring, std::filesystem::path> data)
{
	const Ini ini(std::move(data));
	return !GetNewInpxFolders(ini).empty();
}

bool UpdateCollection(std::map<std::wstring, std::filesystem::path> data)
{
	const Ini ini(std::move(data));
	return UpdateDatabase(ini);
}

}
