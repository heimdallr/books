#pragma warning(push, 0)

#include <filesystem>
#include <fstream>
#include <future>
#include <queue>
#include <set>

#include <QDir>
#include <QFile>

#include <plog/Log.h>

#include "sqlite3ppext.h"

#pragma warning(pop)

#include "constant.h"
#include "types.h"

#include "inpx.h"

#include "util/executor/factory.h"
#include "util/IExecutor.h"

#include "zip.h"

#include "Fb2Parser.h"

using namespace HomeCompa;
using namespace Inpx;

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
		: m_t(std::chrono::high_resolution_clock::now())
		, m_process(std::move(process_))
	{
		PLOGI << m_process << " started";
	}
	~Timer()
	{
		PLOGI << m_process << " done for " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_t).count() << " ms";
	}

	// rule 5
	Timer(const Timer &) = delete;
	Timer(Timer &&) = delete;
	Timer & operator=(const Timer &) = delete;
	Timer & operator=(Timer &&) = delete;

private:
	const std::chrono::steady_clock::time_point m_t;
	const std::wstring m_process;
};

template <typename T>
T & ToLower(T & str)
{
	std::ranges::transform(str, str.begin(), &tolower);
	return str;
}

size_t GetIdDefault(std::wstring_view)
{
	return GetId();
}

Dictionary::const_iterator FindDefault(const Dictionary & container, const std::wstring_view value)
{
	return container.find(value);
}

bool IsComment(const std::wstring_view line)
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
	explicit DatabaseWrapper(const std::filesystem::path & dbFileName, const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
		: m_db(dbFileName.generic_string().data(), flags)
		, m_func(m_db)
	{
		m_db.load_extension("MyHomeLibSQLIteExt");
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
		throw std::invalid_argument(std::format("Cannot open '{}'", genresIniFileName.generic_string()));

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
		genre.dbCode = ToWide(std::format("{:s}.{:03d}", ToMultiByte(parent.dbCode), ++parent.childrenCount));
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

std::set<size_t> ParseItem(const std::wstring_view data, Dictionary & container, const GetIdFunctor & getId = &GetIdDefault, const FindFunctor & find = &FindDefault)
{
	std::set<size_t> result;
	auto it = std::cbegin(data);
	while (it != std::cend(data))
	{
		const auto value = Next(it, std::cend(data), LIST_SEPARATOR);
		result.emplace(Add<size_t>(value, container, getId, find));
	}
	return result;
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

void AddGenres(const BookBuf & buf, Dictionary & genresIndex, Data & data, std::set<size_t> & idGenres)
{
	const auto addGenre = [&index = genresIndex, &genres = data.genres] (std::wstring_view code, std::wstring_view name, const auto parentIt)
	{
		assert(parentIt != index.end() && parentIt->second < std::size(genres));
		const auto itGenre = index.insert(std::make_pair(code, std::size(genres))).first;
		auto & genre = genres.emplace_back(code, genres[parentIt->second].code, name, parentIt->second);
		auto & parentGenre = genres[parentIt->second];
		genre.dbCode = ToWide(std::format("{0}.{1}", ToMultiByte(parentGenre.dbCode), ++parentGenre.childrenCount));
		return itGenre;
	};

	if (buf.date.empty())
	{
		auto itIndex = genresIndex.find(NO_DATE_SPECIFIED);
		if (itIndex == genresIndex.end())
			itIndex = addGenre(L"no_date_specified", NO_DATE_SPECIFIED, genresIndex.find(DATE_ADDED_CODE));
		idGenres.emplace(itIndex->second);
		return;
	}

	auto itDate = std::cbegin(buf.date);
	const auto endDate = std::cend(buf.date);
	const auto year = Next(itDate, endDate, DATE_SEPARATOR);
	const auto month = Next(itDate, endDate, DATE_SEPARATOR);
	const auto dateCode = ToWide(std::format("date_{0}_{1}", ToMultiByte(year), ToMultiByte(month)));

	auto itIndexDate = genresIndex.find(dateCode);
	if (itIndexDate == genresIndex.end())
	{
		const auto yearCode = ToWide(std::format("year_{0}", ToMultiByte(year)));
		auto itIndexYear = genresIndex.find(yearCode);
		if (itIndexYear == genresIndex.end())
			itIndexYear = addGenre(yearCode, year, genresIndex.find(DATE_ADDED_CODE));

		itIndexDate = addGenre(dateCode, std::wstring(year).append(L".").append(month), itIndexYear);
	}
	idGenres.emplace(itIndexDate->second);
}

BookBuf ParseBook(const std::wstring & line)
{
	auto it = std::cbegin(line);
	const auto end = std::cend(line);

	//AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;RATE;KEYWORDS;
	return BookBuf
	{
		.authors = Next(it, end, FIELDS_SEPARATOR),
		.genres = Next(it, end, FIELDS_SEPARATOR),
		.title = Next(it, end, FIELDS_SEPARATOR),
		.seriesName = Next(it, end, FIELDS_SEPARATOR),
		.seriesNum = Next(it, end, FIELDS_SEPARATOR),
		.fileName = Next(it, end, FIELDS_SEPARATOR),
		.size = Next(it, end, FIELDS_SEPARATOR),
		.libId = Next(it, end, FIELDS_SEPARATOR),
		.del = Next(it, end, FIELDS_SEPARATOR),
		.ext = Next(it, end, FIELDS_SEPARATOR),
		.date = Next(it, end, FIELDS_SEPARATOR),
		.lang = Next(it, end, FIELDS_SEPARATOR),
		.rate = Next(it, end, FIELDS_SEPARATOR)
//		.keywords = Next(it, end, FIELDS_SEPARATOR)
	};
}

QString ToString(const Fb2Parser::Data::Authors & authors)
{
	QStringList values;
	values.reserve(static_cast<int>(authors.size()));
	std::ranges::transform(authors, std::back_inserter(values), [] (const Fb2Parser::Data::Author & author)
	{
		return (QStringList() << author.last << author.first << author.middle).join(NAMES_SEPARATOR);
	});
	return values.join(LIST_SEPARATOR) + LIST_SEPARATOR;
}

struct InpxContent
{
	std::vector<std::wstring> collectionInfo;
	std::vector<std::wstring> versionInfo;
	std::vector<std::wstring> inpx;
};

InpxContent ExtractInpxFileNames(const std::filesystem::path & inpxFileName)
{
	if (!exists(inpxFileName))
		return {};

	InpxContent inpxContent;

	const Zip zip(QString::fromStdWString(inpxFileName.generic_wstring()));
	for (const auto & fileName : zip.GetFileNameList())
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

void GetDecodedStream(const Zip & zip, const std::wstring & file, const std::function<void(QIODevice& stream)> & f)
{
	PLOGI << file;
	try
	{
		f(zip.Read(QString::fromStdWString(file)));
	}
	catch (const std::exception & ex)
	{
		PLOGE << file << ": " << ex.what();
	}
	catch (...)
	{
		PLOGE << file << ": unknown error";
	}
}

bool TableExists(sqlite3pp::database & db, const std::string & table)
{
	sqlite3pp::query query(db, std::format("SELECT name FROM sqlite_master WHERE type='table' AND name='{}'", table).data());
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
	const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	tm buf{};
	(void)localtime_s(&buf, &now);
	(void)std::strftime(strDate.data(), strDate.size(), "%Y-%m-%d %H:%M:%S", &buf);

	return data;
}

void ExecuteScript(const std::wstring & action, const std::filesystem::path & dbFileName, const std::filesystem::path & scriptFileName)
{
	Timer t(action);

	DatabaseWrapper db(dbFileName);

	std::ifstream inp(scriptFileName);
	if (!inp.is_open())
		throw std::runtime_error(std::format("Cannot open {}", scriptFileName.generic_string()));

	while(!inp.eof())
	{
		std::string command;
		while (!inp.eof())
		{
			std::string str;
			std::getline(inp, str);
			assert(inp.good() || inp.eof());
			if (str.starts_with("--@@"))
				break;

			if (!str.empty())
				command.append(str).append("\n");
		}

		if (command.empty())
			continue;

		while (command.back() == '\n')
			command.resize(command.size() - 1);

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
size_t StoreRange(const std::filesystem::path & dbFileName, std::string_view process, const std::string_view query, It beg, It end, Functor && f)
{
	const auto rowsTotal = static_cast<size_t>(std::distance(beg, end));
	if (rowsTotal == 0)
		return rowsTotal;

	Timer t(ToWide(std::format("store {0} {1}", process, rowsTotal)));
	size_t rowsInserted = 0;

	DatabaseWrapper db(dbFileName);
	sqlite3pp::transaction tr(db);
	sqlite3pp::command cmd(db, query.data());

	const auto log = [rowsTotal, &rowsInserted]
	{
		PLOGI << std::format("{0} rows inserted ({1}%)", rowsInserted, rowsInserted * 100 / rowsTotal);
	};

	const auto result = std::accumulate(beg, end, size_t { 0 }, [f = std::forward<Functor>(f), &db, &cmd, &rowsInserted, &log](const size_t init, const typename It::value_type & value)
	{
		f(cmd, value);
		const auto localResult = 0
			+ cmd.execute()
			+ cmd.reset()
			;

		if (localResult == 0)
		{
			if (++rowsInserted % LOG_INTERVAL == 0)
				log();
		}
		else
		{
			PLOGE << db->error_code() << ": " << db->error_msg() << std::endl << value;
		}

		return init + localResult;
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

std::wstring RemoveExt(std::wstring & str)
{
	const auto dotPos = str.find_last_of(L'.');
	assert(dotPos != std::string::npos);
	std::wstring result(std::next(std::cbegin(str), static_cast<int>(dotPos) + 1), std::cend(str));
	str.resize(dotPos);
	return result;
}

std::vector<std::wstring> GetNewInpxFolders(const Ini & ini, Data & data)
{
	std::vector<std::wstring> result;

	std::map<std::wstring, std::wstring> dbExt;
	{
		const auto dbFileName = ini(DB_PATH, DEFAULT_DB_PATH).generic_string();
		DatabaseWrapper db(dbFileName.data(), SQLITE_OPEN_READONLY);
		if (!TableExists(db, "Books"))
			return result;

		sqlite3pp::query query(db, "select distinct Folder from Books");
		std::transform(std::begin(query), std::end(query), std::inserter(data.folders, std::end(data.folders)), [&] (const auto & row)
		{
			auto folder = ToWide(row.template get<std::string>(0));
			auto ext = RemoveExt(ToLower(folder));
			dbExt.emplace(folder, std::move(ext));
			return folder;
		});
	}

	std::map<std::wstring, std::wstring> inpxExt;
	std::set<std::wstring> inpxFolders;
	std::ranges::transform(ExtractInpxFileNames(ini(INPX, DEFAULT_INPX)).inpx, std::inserter(inpxFolders, std::end(inpxFolders)), [&inpxExt] (std::wstring item)
	{
		auto ext = RemoveExt(ToLower(item));
		inpxExt.emplace(item, std::move(ext));
		return item;
	});

	std::ranges::set_difference(inpxFolders, data.folders, std::back_inserter(result));
	std::ranges::transform(result, std::begin(result), [&inpxExt] (const std::wstring & item)
	{
		const auto it = inpxExt.find(item);
		assert(it != inpxExt.end());
		return item + L'.' + it->second;
	});

	const auto folders = std::move(data.folders);
	data.folders = {};
	std::ranges::transform(folders, std::inserter(data.folders, data.folders.end()), [&dbExt] (const std::wstring & item)
	{
		const auto it = dbExt.find(item);
		assert(it != dbExt.end());
		return item + L'.' + it->second;
	});

	return result;
}

Dictionary ReadDictionary(const std::string_view name, sqlite3pp::database & db, const char * statement)
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

class IPool  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IPool() = default;
	virtual void Work(std::promise<void> & startPromise) = 0;
};

class Thread
{
	NON_COPY_MOVABLE(Thread)

public:
	explicit Thread(IPool & pool)
		: m_thread(&IPool::Work, std::ref(pool), std::ref(m_startPromise))
	{
		m_startPromise.get_future().get();
	}

	~Thread()
	{
		if (m_thread.joinable())
			m_thread.join();
	}

private:
	std::promise<void> m_startPromise;
	std::thread m_thread;
};

}

class Parser::Impl final
	: virtual public IPool
{
public:
	Impl(Ini ini, const CreateCollectionMode mode, Callback callback)
		: m_ini(std::move(ini))
		, m_mode(mode)
		, m_callback(std::move(callback))
		, m_executor(Util::ExecutorFactory::Create(Util::ExecutorImpl::Async))
	{
	}

	void Process()
	{
		(*m_executor)({ "Create collection", [&]
		{
			ProcessImpl();
			return [&] (size_t)
			{
				m_callback(true);
			};
		} });
	}

	void UpdateDatabase()
	{
		(*m_executor)({ "Update collection", [&]
		{
			UpdateDatabaseImpl();
			return [&] (size_t)
			{
				m_callback(true);
			};
		} });
	}

private: // IPool
	void Work(std::promise<void> & startPromise) override
	{
		startPromise.set_value();

		while(true)
		{
			std::wstring folder;
			{
				std::unique_lock lock(m_foldersToParseGuard);
				if (m_foldersToParse.empty())
					break;

				folder = std::move(m_foldersToParse.front());
				m_foldersToParse.pop();
				if (m_foldersToParse.empty())
					m_foldersToParseCondition.notify_one();
			}
			try
			{
				PLOGI << folder;
				std::set<std::string> files;
				const QFileInfo archiveFileInfo(QString::fromStdWString(m_rootFolder / folder));
				for (const Zip zip(archiveFileInfo.filePath()); const auto & fileName : zip.GetFileNameList())
				{
					if (QFileInfo(fileName).suffix() == "fb2")
					{
						try
						{
							ParseFile(files, folder, zip, fileName, archiveFileInfo.birthTime());
						}
						catch (const std::exception & ex)
						{
							PLOGE << fileName << ": " << ex.what();
						}
						catch (...)
						{
							PLOGE << fileName << ": unknown error";
						}
					}
				}
			}
			catch (const std::exception & ex)
			{
				PLOGE << folder << ": " << ex.what();
			}
			catch (...)
			{
				PLOGE << folder << ": unknown error";
			}
		}
	}

private:
	void ProcessImpl()
	{
		Timer t(L"work");

		const auto & dbFileName = m_ini(DB_PATH, DEFAULT_DB_PATH);

		auto settingsTableData = ReadSettings(dbFileName);
		ExecuteScript(L"create database", dbFileName, m_ini(DB_CREATE_SCRIPT, DEFAULT_DB_CREATE_SCRIPT));

		Parse(std::move(settingsTableData));
		if (const auto failsCount = Store(dbFileName, m_data); failsCount != 0)
			PLOGE << "Something went wrong";

		ExecuteScript(L"update database", dbFileName, m_ini(DB_UPDATE_SCRIPT, DEFAULT_DB_UPDATE_SCRIPT));
	}

	void UpdateDatabaseImpl()
	{
		const auto & dbFileName = m_ini(DB_PATH, DEFAULT_DB_PATH);
		const auto [oldData, oldGenresIndex] = ReadData(dbFileName, m_ini(GENRES, DEFAULT_GENRES));

		m_data = oldData;
		m_genresIndex = oldGenresIndex;
		m_unknownGenreId = m_genresIndex.find(UNKNOWN)->second;

		const auto & inpxFileName = m_ini(INPX, DEFAULT_INPX);
		const Zip zip(QString::fromStdWString(inpxFileName));
		ParseInpxFiles(inpxFileName, &zip, GetNewInpxFolders(m_ini, m_data));

		const auto filter = [] (Dictionary & dst, const Dictionary & src)
		{
			for (auto it = dst.begin(); it != dst.end(); )
				if (src.contains(it->first))
					it = dst.erase(it);
				else
					++it;
		};

		filter(m_data.authors, oldData.authors);
		filter(m_data.series, oldData.series);

		if (const auto failsCount = Store(dbFileName, m_data); failsCount != 0)
			PLOGE << "Something went wrong";
	}

	void Parse(SettingsTableData && settingsTableData)
	{
		Timer t(L"parsing archives");

		m_data.settings = std::move(settingsTableData);

		auto [genresData, genresIndex] = LoadGenres(m_ini(GENRES, DEFAULT_GENRES));
		m_data.genres = std::move(genresData);
		m_genresIndex = std::move(genresIndex);
		m_unknownGenreId = m_genresIndex.find(UNKNOWN)->second;

		const std::filesystem::path & inpxFileName = m_ini(INPX, DEFAULT_INPX);
		const auto inpxContent = ExtractInpxFileNames(inpxFileName);

		const auto zip = [&]
		{
			if (!exists(inpxFileName))
				return std::unique_ptr<Zip>{};

			try
			{
				return std::make_unique<Zip>(QString::fromStdWString(inpxFileName.generic_wstring()));
			}
			catch(...){}
			return std::unique_ptr<Zip>{};
		}();

		for (const auto & fileName : inpxContent.collectionInfo)
			GetDecodedStream(*zip, fileName, [&] (QIODevice & zipDecodedStream)
		{
			ProcessCollectionInfo(zipDecodedStream, m_data.settings);
		});

		for (const auto & fileName : inpxContent.versionInfo)
			GetDecodedStream(*zip, fileName, [&] (QIODevice & zipDecodedStream)
		{
			ProcessVersionInfo(zipDecodedStream, m_data.settings);
		});

		ParseInpxFiles(inpxFileName, zip.get(), inpxContent.inpx);
	}

	void ParseInpxFiles(const std::filesystem::path & inpxFileName, const Zip * zipInpx, const std::vector<std::wstring> & inpxFiles)
	{
		m_rootFolder = std::filesystem::path(inpxFileName).parent_path();
		if (zipInpx)
		{
			for (const auto & fileName : inpxFiles)
				GetDecodedStream(*zipInpx, fileName, [&] (QIODevice & zipDecodedStream)
			{
				ProcessInpx(zipDecodedStream, m_rootFolder, fileName);
			});

			PLOGI << m_n << " rows parsed";
		}

		if (!!(m_mode & CreateCollectionMode::ScanUnIndexedFolders))
		{
			for (auto const & entry : std::filesystem::recursive_directory_iterator(m_rootFolder))
			{
				if (entry.is_directory())
					continue;

				auto folder = entry.path().wstring();
				folder.erase(0, m_rootFolder.string().size() + 1);
				ToLower(folder);

				if (const auto ext = entry.path().extension(); ext != ".zip" && ext != ".7z")
					continue;

				if (m_data.folders.contains(folder))
					continue;

				m_foldersToParse.push(std::move(folder));
			}

			if (!m_foldersToParse.empty())
			{
				const auto cpuCount = static_cast<int>(std::thread::hardware_concurrency());
				const auto maxThreadCount = std::max(std::min(cpuCount - 2, static_cast<int>(m_foldersToParse.size())), 1);
				std::generate_n(std::back_inserter(m_threads), maxThreadCount, [&] { return std::make_unique<Thread>(*this); });

				while (true)
				{
					std::unique_lock lock(m_foldersToParseGuard);
					m_foldersToParseCondition.wait(lock, [&]
					{
						return m_foldersToParse.empty();
					});

					if (m_foldersToParse.empty())
						break;
				}

				m_threads.clear();
			}
		}

		LogErrors();
	}

	void ProcessInpx(QIODevice & stream, const std::filesystem::path & rootFolder, std::wstring folder)
	{
		const auto mask = QString::fromStdWString(std::filesystem::path(folder).replace_extension("*"));
		QStringList suitableFiles = QDir(QString::fromStdWString(rootFolder)).entryList({ mask });
		std::ranges::transform(suitableFiles, suitableFiles.begin(), [] (const auto & file)
		{
			return file.toLower();
		});
		if (const auto [begin, end] = std::ranges::remove_if(suitableFiles, [] (const auto & file)
		{
			const auto ext = QFileInfo(file).suffix();
			return ext != "zip" && ext != "7z";
		}); begin != end)
			suitableFiles.erase(begin, end);
		folder = *m_data.folders.insert(suitableFiles.isEmpty() ? std::filesystem::path(folder).replace_extension(ZIP).wstring() : suitableFiles.front().toStdWString()).first;

		std::set<std::string> files;

		while (true)
		{
			const auto byteArray = stream.readLine();
			if (byteArray.isEmpty())
				break;

			const auto line = ToWide(byteArray.constData());
			const auto buf = ParseBook(line);
			AddBook(files, buf, folder);
		}

		const QFileInfo archiveFileInfo(QString::fromStdWString(rootFolder / folder));
		if (!archiveFileInfo.exists())
		{
			PLOGW << archiveFileInfo.fileName() << " not found";
			return;
		}

		for (const Zip zip(archiveFileInfo.filePath()); const auto & fileName : zip.GetFileNameList())
		{
			if (files.contains(fileName.toLower().toStdString()))
				continue;

			PLOGW << "Book is not indexed: " << ToMultiByte(folder) << "/" << fileName;
			if (!!(m_mode & CreateCollectionMode::AddUnIndexedFiles))
				ParseFile(files, folder, zip, fileName, archiveFileInfo.birthTime());
		}
	}

	void ParseFile(std::set<std::string> & files, const std::wstring & folder, const Zip & zip, const QString & fileName, const QDateTime & zipDateTime)
	{
		QFileInfo fileInfo(fileName);
		auto & stream = zip.Read(fileName);
		Fb2Parser parser(stream, fileName);
		const auto parserData = parser.Parse();
		PLOGI_IF(++m_parsedN % LOG_INTERVAL == 0) << m_parsedN << " books parsed";

		if (!parserData.error.isEmpty())
		{
			std::lock_guard lock(m_dataGuard);
			m_errors.push_back(QString("%1/%2: %3").arg(QString::fromStdWString(folder), fileName, parserData.error));
			return;
		}

		const auto & fileDateTime = zip.GetFileTime(fileName);
		auto dateTime = (fileDateTime.isValid() ? fileDateTime : zipDateTime).toString("yyyy-MM-dd");

		const auto values = QStringList()
			<< ToString(parserData.authors)
			<< parserData.genres.join(LIST_SEPARATOR) + LIST_SEPARATOR
			<< parserData.title
			<< parserData.series
			<< QString::number(parserData.seqNumber)
			<< fileInfo.completeBaseName()
			<< QString::number(zip.GetFileSize(fileName))
			<< fileInfo.completeBaseName()
			<< "0"
			<< fileInfo.suffix()
			<< std::move(dateTime)
			<< parserData.lang
			<< "0"
			;

		const auto line = values.join(FIELDS_SEPARATOR).toStdWString();
		const auto buf = ParseBook(line);

		std::lock_guard lock(m_dataGuard);
		AddBook(files, buf, folder);
	}

	void AddBook(std::set<std::string> & files, const BookBuf & buf, const std::wstring & folder)
	{
		const auto id = GetId();
		auto file = ToMultiByte(buf.fileName) + "." + ToMultiByte(buf.ext);
		files.emplace(ToLower(file));

		for (const auto idAuthor : ParseItem(buf.authors, m_data.authors))
			m_data.booksAuthors.emplace_back(id, idAuthor);

		auto idGenres = ParseItem(buf.genres, m_genresIndex,
			[&, &data = m_data.genres] (std::wstring_view newItemTitle)
			{
				const auto result = std::size(data);
				auto & genre = data.emplace_back(newItemTitle, L"", newItemTitle, m_unknownGenreId);
				auto & unknownGenre = data[m_unknownGenreId];
				genre.dbCode = ToWide(std::format("{0}.{1}", ToMultiByte(unknownGenre.dbCode), ++unknownGenre.childrenCount));
				m_unknownGenres.push_back(genre.name);
				return result;
			},
			[&data = m_data.genres] (const Dictionary & container, std::wstring_view value)
			{
				const auto itGenre = container.find(value);
				return itGenre != container.end() ? itGenre : std::ranges::find_if(container, [value, &data] (const auto & item)
				{
					return IsStringEqual(value, data[item.second].name);
				});
			}
		);

		AddGenres(buf, m_genresIndex, m_data, idGenres);

		std::ranges::transform(idGenres, std::back_inserter(m_data.booksGenres), [&] (const size_t idGenre)
		{
			return std::make_pair(id, idGenre);
		});

		m_data.books.emplace_back(id, buf.libId, buf.title, Add<int, -1>(buf.seriesName, m_data.series), To<int>(buf.seriesNum, -1), buf.date, To<int>(buf.rate), buf.lang, folder, buf.fileName, files.size() - 1, buf.ext, To<size_t>(buf.size), To<bool>(buf.del, false)/*, keywords*/);

		PLOGI_IF((++m_n % LOG_INTERVAL) == 0) << m_n << " books added";
	}

	void LogErrors() const
	{
		if (!std::empty(m_unknownGenres))
		{
			PLOGW << "Unknown genres:";
			for (const auto & genre : m_unknownGenres)
				PLOGW << genre;
		}

		if (!std::empty(m_errors))
		{
			PLOGE << "Parsing skipped due to errors:";
			for (const auto & error : m_errors)
				PLOGE << error;
		}
	}

private:
	const Ini m_ini;
	const CreateCollectionMode m_mode;
	const Callback m_callback;
	std::unique_ptr<Util::IExecutor> m_executor;

	std::filesystem::path m_rootFolder;
	Data m_data;
	Dictionary m_genresIndex;
	size_t m_n { 0 };
	std::atomic_uint64_t m_parsedN { 0 };
	std::vector<std::wstring> m_unknownGenres;
	size_t m_unknownGenreId { 0 };

	std::vector<QString> m_errors;

	std::queue<std::wstring> m_foldersToParse;
	std::mutex m_foldersToParseGuard;
	std::condition_variable m_foldersToParseCondition;

	std::mutex m_dataGuard;

	std::vector<std::unique_ptr<Thread>> m_threads;
};

Parser::Parser() = default;
Parser::~Parser() = default;

void Parser::CreateNewCollection(std::map<std::wstring, std::filesystem::path> data, const CreateCollectionMode mode, Callback callback)
{
	try
	{
		std::make_unique<Impl>(Ini(std::move(data)), mode, std::move(callback)).swap(m_impl);
		m_impl->Process();
	}
	catch (const std::exception & ex)
	{
		PLOGE << ex.what();
	}
	catch (...)
	{
		PLOGE << "unknown error";
	}
}

void Parser::UpdateCollection(std::map<std::wstring, std::filesystem::path> data, const CreateCollectionMode mode, Callback callback)
{
	try
	{
		std::make_unique<Impl>(Ini(std::move(data)), mode, std::move(callback)).swap(m_impl);
		m_impl->UpdateDatabase();
	}
	catch (const std::exception & ex)
	{
		PLOGE << ex.what();
	}
	catch (...)
	{
		PLOGE << "unknown error";
	}
}
