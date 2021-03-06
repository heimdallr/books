__pragma(warning(push, 0))

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <numeric>
#include <unordered_map>

#include "ZipLib/ZipFile.h"
#include "sqlite/shell/sqlite_shell.h"
#include "sqlite3ppext.h"
#include "fmt/core.h"

#include <Windows.h>

__pragma(warning(pop))

#include "Configuration.h"

namespace {

constexpr std::string_view INP_EXT = ".inp";
constexpr char FIELDS_SEPARATOR = '\x04';
constexpr char INI_SEPARATOR = '=';
constexpr char LIST_SEPARATOR = ':';
constexpr char NAMES_SEPARATOR = ',';
constexpr char GENRE_SEPARATOR = '|';

constexpr const char * GENRES      = "genres";
constexpr const char * INI_EXT     = "ini";
constexpr const char * INPX        = "inpx";
constexpr const char * UNKNOWN     = "unknown";
constexpr const char * DB          = "db";

constexpr size_t LOG_INTERVAL = 10000;

size_t g_id = 0;
size_t GetId()
{
	return ++g_id;
}

class Timer
{
public:
	explicit Timer(std::string process_)
		: t(std::chrono::high_resolution_clock::now())
		, process(std::move(process_))
	{
		std::cout << process << " started" << std::endl;
	}
	~Timer()
	{
		std::cout << process << " done for " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - t).count() << " ms" << std::endl;
	}

	// rule 5
	Timer(const Timer &) = delete;
	Timer(Timer &&) = delete;
	Timer & operator=(const Timer &) = delete;
	Timer & operator=(Timer &&) = delete;

private:
	const std::chrono::steady_clock::time_point t;
	const std::string process;
};

struct StringHash
{
	using hash_type = std::hash<std::string_view>;
	// ReSharper disable once CppTypeAliasNeverUsed
	using is_transparent = void;

	int64_t operator()(const char *        str) const { return static_cast<int64_t>(hash_type{}(str)); }
	int64_t operator()(std::string_view    str) const { return static_cast<int64_t>(hash_type{}(str)); }
	int64_t operator()(std::string const & str) const { return static_cast<int64_t>(hash_type{}(str)); }
};

struct Book
{
	Book(size_t id_
		, std::string_view libId_
		, std::string_view title_
		, int seriesId_
		, int seriesNum_
		, std::string_view date_
		, int rate_
		, std::string_view language_
		, std::string_view folder_
		, std::string_view fileName_
		, size_t insideNo_
		, std::string_view format_
		, size_t size_
		, bool isDeleted_
		, std::string_view keywords_
	)
		: id(id_)
		, libId(libId_)
		, title(title_)
		, seriesId(seriesId_)
		, seriesNum(seriesNum_)
		, date(date_)
		, rate(rate_)
		, language(language_)
		, folder(folder_)
		, fileName(fileName_)
		, insideNo(insideNo_)
		, format(format_)
		, size(size_)
		, isDeleted(isDeleted_)
		, keywords(keywords_)
	{
	}

	size_t      id;
	std::string libId;
	std::string title;
	int         seriesId;
	int         seriesNum;
	std::string date;
	int         rate;
	std::string language;
	std::string folder;
	std::string fileName;
	size_t      insideNo;
	std::string format;
	size_t      size;
	bool        isDeleted;
	std::string keywords;
};

const std::locale g_utf8("ru_RU.UTF-8");

std::string toLower(std::string multiByteStr)
{
	const auto size = [](const auto & str) { return static_cast<int>(str.size()); };

	const auto wideSize = static_cast<std::wstring::size_type>(MultiByteToWideChar(CP_UTF8, 0, multiByteStr.data(), size(multiByteStr), nullptr, 0));
	std::wstring wideString(wideSize, 0);
	MultiByteToWideChar(CP_UTF8, 0, multiByteStr.data(), size(multiByteStr), wideString.data(), size(wideString));

	std::ranges::transform(std::as_const(wideString), std::begin(wideString), [](auto c)
	{
		return std::tolower(c, g_utf8);
	});

	const auto multiByteSize = static_cast<std::wstring::size_type>(WideCharToMultiByte(CP_UTF8, 0, wideString.data(), size(wideString), nullptr, 0, nullptr, nullptr));
	multiByteStr.resize(multiByteSize);
	WideCharToMultiByte(CP_UTF8, 0, wideString.data(), size(wideString), multiByteStr.data(), size(multiByteStr), nullptr, nullptr);

	return multiByteStr;
}

struct Genre
{
	std::string code;
	std::string parentCore;
	std::string name;
	size_t parentId;
	std::string dbCode;

	std::string nameLower;
	size_t childrenCount{ 0 };

	Genre(std::string_view code_, std::string_view parentCode_, std::string_view name_, size_t parentId_ = 0)
		: code(code_)
		, parentCore(parentCode_)
		, name(name_)
		, parentId(parentId_)
		, nameLower(toLower(name))
	{
	}
};
	
using Books = std::vector<Book>;
using Dictionary = std::unordered_map<std::string, size_t, StringHash, std::equal_to<>>;
using Genres = std::vector<Genre>;
using Links = std::vector<std::pair<size_t, size_t>>;
using SettingsTableData = std::unordered_map<int, std::string>;

using GetIdFunctor = std::function<size_t(std::string_view)>;
using FindFunctor = std::function<Dictionary::iterator(Dictionary &, std::string_view)>;

size_t GetIdDefault(std::string_view)
{
	return GetId();
}

Dictionary::iterator FindDefault(Dictionary & container, std::string_view value)
{
	return container.find(value);
}

struct Data
{
	Books books;
	Dictionary authors, series;
	Genres genres;
	Links booksAuthors, booksGenres;
};

template <typename It>
std::string_view Next(It & beg, const It end, const char separator)
{
	auto next = std::find(beg, end, separator);
	const std::string_view result(beg, next);
	beg = next != end ? std::next(next) : end;
	return result;
}

template<typename T>
T To(std::string_view value, T defaultValue = 0)
{
	char * p_end;
	return value.empty()
		? defaultValue
		: static_cast<T>(std::strtoull(value.data(), &p_end, 10));
}

class Ini
{
public:
	explicit Ini(const std::filesystem::path & path)
	{
		if (!exists(path))
			throw std::invalid_argument("Need inpx file as command line argument");

		std::string line;

		std::ifstream iniStream(path);
		while (std::getline(iniStream, line))
		{
			auto it = std::cbegin(line);
			const auto key = Next(it, std::cend(line), INI_SEPARATOR);
			const auto value = Next(it, std::cend(line), INI_SEPARATOR);
			_data.emplace(key, value);
		}
	}

	const std::string & operator()(const char * key) const
	{
		const auto it = _data.find(key);
		if (it == _data.end())
			throw std::invalid_argument(fmt::format("Cannot find key {}", key));

		return it->second;
	}

private:
	std::unordered_map<std::string, std::string> _data;
};

auto LoadGenres(std::string_view genresIniFileName)
{
	Genres genres;
	Dictionary index;

	std::ifstream iniStream(genresIniFileName);
	if (!iniStream.is_open())
		throw std::invalid_argument(fmt::format("Cannot open '{}'", genresIniFileName));

	index.emplace("", std::size(genres));
	auto & root = genres.emplace_back("", "", "");
	root.dbCode = "0";

	std::string line;
	while (std::getline(iniStream, line))
	{
		auto it = std::cbegin(line);
		const auto codes = Next(it, std::cend(line), GENRE_SEPARATOR);
		auto itCode = std::cbegin(codes);
		std::string code;
		while(itCode != std::cend(codes))
		{
			const auto & added = index.emplace(Next(itCode, std::cend(codes), LIST_SEPARATOR), std::size(genres)).first->first;
			if (code.empty())
				code = added;
		}

		const auto parent = Next(it, std::cend(line), GENRE_SEPARATOR);
		const auto title  = Next(it, std::cend(line), GENRE_SEPARATOR);
		genres.emplace_back(code, parent, title);
	}

	std::for_each(std::next(std::begin(genres)), std::end(genres), [&index, &genres](Genre & genre)
	{
		const auto it = index.find(genre.parentCore);
		assert(it != index.end());
		auto & parent = genres[it->second];
		genre.parentId = it->second;
		genre.dbCode = fmt::format("{0}.{1}", parent.dbCode, ++parent.childrenCount);
	});

	return std::make_pair(std::move(genres), std::move(index));
}

template<typename T, T emptyValue = 0>
T Add(std::string_view value, Dictionary & container, const GetIdFunctor & getId = &GetIdDefault, const FindFunctor & find = &FindDefault)
{
	if (value.empty())
		return emptyValue;

	auto it = find(container, value);
	if (it == container.end())
		it = container.emplace(value, getId(value)).first;

	return static_cast<T>(it->second);
}

void ParseItem(size_t id, std::string_view data, Dictionary & container, Links & links, const GetIdFunctor & getId = &GetIdDefault, const FindFunctor & find = &FindDefault)
{
	auto it = std::cbegin(data);
	while (it != std::cend(data))
	{
		const auto value = Next(it, std::cend(data), LIST_SEPARATOR);
		links.emplace_back(id, Add<size_t>(value, container, getId, find));
	}
}

Data Parse(std::string_view genresName, std::string_view inpxFileName)
{
	Timer t("parsing archives");

	Data data;
	auto [genresData, genresIndex] = LoadGenres(genresName);
	data.genres = std::move(genresData);
	const auto unknownGenreId = genresIndex.find(UNKNOWN)->second;
	auto & unknownGenre = data.genres[unknownGenreId];

	std::vector<std::string> unknownGenres;

	const auto archive = ZipFile::Open(inpxFileName.data());
	const auto entriesCount = archive->GetEntriesCount();
	size_t n = 0;

	for (size_t i = 0; i < entriesCount; ++i)
	{
		const auto entry = archive->GetEntry(static_cast<int>(i));
		const auto folder = entry->GetFullName();

		if (!folder.ends_with(INP_EXT))
		{
			std::cout << folder << " skipped" << std::endl;
			continue;
		}

		std::cout << folder << std::endl;

		auto * const stream = entry->GetDecompressionStream();
		assert(stream);

		size_t insideNo = 0;
		std::string line;		
		while (std::getline(*stream, line))
		{
			const auto id = GetId();

			auto itBook = std::cbegin(line);
			//AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;RATE;KEYWORDS;
			const auto authors    = Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto genres     = Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto title      = Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto seriesName = Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto seriesNum  = Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto fileName   = Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto size       = Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto libId      = Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto del        = Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto ext        = Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto date       = Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto lang       = Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto rate       = Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto keywords   = Next(itBook, std::cend(line), FIELDS_SEPARATOR);

			ParseItem(id, authors, data.authors, data.booksAuthors);
			ParseItem(id, genres, genresIndex, data.booksGenres
				, [unknownGenreId, &unknownGenre, &unknownGenres, &data = data.genres](std::string_view title)
					{
						const auto result = std::size(data);
						auto & genre = data.emplace_back(title, "", title, unknownGenreId);
						genre.dbCode = fmt::format("{0}.{1}", unknownGenre.dbCode, ++unknownGenre.childrenCount);
						unknownGenres.push_back(genre.nameLower);
						return result;
					}
				, [&data = data.genres](Dictionary & container, std::string_view value)
					{
						const auto it = container.find(value);
						const auto v = toLower(std::string(std::cbegin(value), std::cend(value)));
						return it != container.end() ? it : std::ranges::find_if(container, [&data, &v](const Dictionary::value_type & item)
						{
							return v == data[item.second].nameLower;
						});
					}
			);

			data.books.emplace_back(id, libId, title, Add<int, -1>(seriesName, data.series), To<int>(seriesNum, -1), date, To<int>(rate), lang, folder, fileName, insideNo++, ext, To<size_t>(size), To<bool>(del, false), keywords);

			if ((++n % LOG_INTERVAL) == 0)
				std::cout << n << " rows parsed" << std::endl;
		}
	}

	std::cout << "total " << n << " rows parsed" << std::endl;

	if (!std::empty(unknownGenres))
	{
		std::cerr << "Unknown genres" << std::endl;
		std::copy(std::cbegin(unknownGenres), std::cend(unknownGenres), std::ostream_iterator<std::string>(std::cerr, "\n"));
	}

	return data;
}

Ini ParseConfig(int argc, char * argv[])
{
	return Ini(argc < 2 ? std::filesystem::path(argv[0]).replace_extension(INI_EXT) : std::filesystem::path(argv[1]));
}

bool TableExists(sqlite3pp::database & db, const std::string & table)
{
	sqlite3pp::query query(db, fmt::format("SELECT name FROM sqlite_master WHERE type='table' AND name='{}'", table).data());
	return std::begin(query) != std::end(query);
}

SettingsTableData ReadSettings(const std::string & dbFileName)
{
	SettingsTableData data;

	sqlite3pp::database db(dbFileName.data());
	if (!TableExists(db, "Settings"))
		return {};

	sqlite3pp::query query(db, "select SettingID, SettingValue from Settings");

	std::transform(std::begin(query), std::end(query), std::inserter(data, std::end(data)), [](const auto & row)
	{
		int id;
		const char * value;
		std::tie(id, value) = row.template get_columns<int, char const *>(0, 1);
		return std::make_pair(id, std::string(value));
	});

	return data;
}

void ReCreateDatabase(std::string dbFileName)
{
	std::cout << "Recreating database" << std::endl;

	char stubExe[] = "stub.exe";
	auto readScript = fmt::format(".read {}.", CREATE_COLLECTION_SCRIPT);
	auto loadExt = fmt::format(".load {}", MHL_SQLITE_EXTENSION);

	char * v[]
	{
		stubExe,
		dbFileName.data(),
		loadExt.data(),
		readScript.data(),
	};

	SQLiteShellExecute(static_cast<int>(std::size(v)), v);
}

template<typename It, typename Functor>
int64_t StoreRange(std::string dbFileName, std::string query, It beg, It end, Functor && f)
{
	sqlite3pp::database db(dbFileName.data());	
	db.load_extension(MHL_SQLITE_EXTENSION);
	sqlite3pp::ext::function func(db);
	func.create("MHL_TRIGGERS_ON", [](sqlite3pp::ext::context & ctx) { ctx.result(1); });

	sqlite3pp::transaction tr(db);
	sqlite3pp::command cmd(db, query.data());

	const auto result = std::accumulate(beg, end, int64_t{ 0 }, [f = std::forward<Functor>(f), &cmd](int64_t init, const typename It::value_type & value)
	{
		f(cmd, value);
		return init
			+ cmd.execute()
			+ cmd.reset()
			;
	});
	tr.commit();

	return result;
}

int64_t Store(const std::string & dbFileName, const Data & data)
{
	int64_t result = 0;
	result += StoreRange(dbFileName, "INSERT INTO Authors (AuthorID, LastName, FirstName, MiddleName) VALUES(?, ?, ?, ?)", std::cbegin(data.authors), std::cend(data.authors), [](sqlite3pp::command & cmd, const Dictionary::value_type & item)
	{		
		const auto & [author, id] = item;
		auto it = std::cbegin(author);
		const auto last   = Next(it, std::cend(author), NAMES_SEPARATOR);
		const auto first  = Next(it, std::cend(author), NAMES_SEPARATOR);
		const auto middle = Next(it, std::cend(author), NAMES_SEPARATOR);
		cmd.binder() << id << last << first << middle;
	});

	result += StoreRange(dbFileName, "INSERT INTO Series (SeriesID, SeriesTitle) VALUES (?, ?)", std::cbegin(data.series), std::cend(data.series), [](sqlite3pp::command & cmd, const Dictionary::value_type & item)
	{
		cmd.binder() << item.second << item.first;
	});

	result += StoreRange(dbFileName, "INSERT INTO Genres (GenreCode, ParentCode, FB2Code, GenreAlias) VALUES(?, ?, ?, ?)", std::next(std::cbegin(data.genres)), std::cend(data.genres), [&genres = data.genres](sqlite3pp::command & cmd, const Genre & genre)
	{
		cmd.binder() << genre.dbCode << genres[genre.parentId].dbCode << genre.code << genre.name;
	});

	const char * queryText = "INSERT INTO Books ("
		"BookID   , LibID     , Title    , SeriesID, "
		"SeqNumber, UpdateDate, LibRate  , Lang    , "
		"Folder   , FileName  , InsideNo , Ext     , "
		"BookSize , IsLocal   , IsDeleted, KeyWords"
		") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
	result += StoreRange(dbFileName, queryText, std::cbegin(data.books), std::cend(data.books), [](sqlite3pp::command & cmd, const Book & book)
	{
																		   cmd.bind( 1, book.id);
																		   cmd.bind( 2, book.libId, sqlite3pp::nocopy);
																		   cmd.bind( 3, book.title, sqlite3pp::nocopy);
			book.seriesId  == -1  ? cmd.bind( 4, sqlite3pp::null_type()) : cmd.bind( 4, book.seriesId);
			book.seriesNum == -1  ? cmd.bind( 5, sqlite3pp::null_type()) : cmd.bind( 5, book.seriesNum);
																		   cmd.bind( 6, book.date, sqlite3pp::nocopy);
																		   cmd.bind( 7, book.rate);
																		   cmd.bind( 8, book.language, sqlite3pp::nocopy);
																		   cmd.bind( 9, book.folder, sqlite3pp::nocopy);
																		   cmd.bind(10, book.fileName, sqlite3pp::nocopy);
																		   cmd.bind(11, book.insideNo);
																		   cmd.bind(12, book.format, sqlite3pp::nocopy);
																		   cmd.bind(13, book.size);
																		   cmd.bind(14, 1);
																		   cmd.bind(15, book.isDeleted ? 1 : 0);
			book.keywords.empty() ? cmd.bind(16, sqlite3pp::null_type()) : cmd.bind(16, book.keywords, sqlite3pp::nocopy);
	});

	result += StoreRange(dbFileName, "INSERT INTO Author_List (AuthorID, BookID) VALUES(?, ?)", std::cbegin(data.booksAuthors), std::cend(data.booksAuthors), [](sqlite3pp::command & cmd, const Links::value_type & item)
	{
		cmd.binder() << item.second << item.first;
	});

	result += StoreRange(dbFileName, "INSERT INTO Genre_List (BookID, GenreCode) VALUES(?, ?)", std::cbegin(data.booksGenres), std::cend(data.booksGenres), [&genres = data.genres](sqlite3pp::command & cmd, const Links::value_type & item)
	{
		assert(item.second < std::size(genres));
		cmd.binder() << item.first << genres[item.second].dbCode;
	});

	return result;
}

}

void mainImpl(int argc, char * argv[])
{
	const auto ini = ParseConfig(argc, argv);
	const auto dbFileName = ini(DB);

	const auto settingsTableData = ReadSettings(dbFileName);
	ReCreateDatabase(dbFileName);

	const auto data = Parse(ini(GENRES), ini(INPX));
	Store(dbFileName, data);
}

int main(int argc, char* argv[])
{
	try
	{
		Timer t("work");
		mainImpl(argc, argv);
		return 0;
	}
	catch (const std::exception & ex)
	{
		std::cerr << ex.what();
	}
	catch(...)
	{
		std::cerr << "unknown error";
	}
	return 1;
}
