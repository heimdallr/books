__pragma(warning(push, 0))

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <numeric>

#include "ZipLib/ZipFile.h"
#include "sqlite/shell/sqlite_shell.h"
#include "sqlite3ppext.h"
#include "fmt/core.h"

#include <Windows.h>

__pragma(warning(pop))

#include "Configuration.h"

namespace {

constexpr wchar_t FIELDS_SEPARATOR = '\x04';
constexpr wchar_t GENRE_SEPARATOR  = '|';
constexpr wchar_t INI_SEPARATOR    = '=';
constexpr wchar_t LIST_SEPARATOR   = ':';
constexpr wchar_t NAMES_SEPARATOR  = ',';

constexpr const wchar_t * COLLECTION_INFO  = L"collection.info";
constexpr const wchar_t * CREATE_DB_SCRIPT = L"create_db_script";
constexpr const wchar_t * DB_PATH          = L"db_path";
constexpr const wchar_t * GENRES           = L"genres";
constexpr const wchar_t * INI_EXT          = L"ini";
constexpr const wchar_t * INPX             = L"inpx";
constexpr const wchar_t * MHL_TRIGGERS_ON  = L"mhl_triggers_on";
constexpr const wchar_t * VERSION_INFO     = L"version.info";
constexpr const wchar_t * ZIP              = L"zip";

constexpr const char * SCHEMA_VERSION_VALUE = "{FEC8CB6F-300A-4b92-86D1-7B40867F782B}";

constexpr std::wstring_view INP_EXT = L".inp";
constexpr std::wstring_view UNKNOWN = L"unknown";

constexpr size_t LOG_INTERVAL = 10000;

[[maybe_unused]] constexpr uint32_t PROP_CLASS_SYSTEM     = 0x10000000;
[[maybe_unused]] constexpr uint32_t PROP_CLASS_COLLECTION = 0x20000000;
[[maybe_unused]] constexpr uint32_t PROP_CLASS_BOTH       = PROP_CLASS_SYSTEM | PROP_CLASS_COLLECTION;
[[maybe_unused]] constexpr uint32_t PROP_CLASS_MASK       = 0xF0000000;

[[maybe_unused]] constexpr uint32_t PROP_TYPE_INTEGER  = 0x00010000;
[[maybe_unused]] constexpr uint32_t PROP_TYPE_DATETIME = 0x00020000;
[[maybe_unused]] constexpr uint32_t PROP_TYPE_BOOLEAN  = 0x00030000;
[[maybe_unused]] constexpr uint32_t PROP_TYPE_STRING   = 0x00040000;
[[maybe_unused]] constexpr uint32_t PROP_TYPE_MASK     = 0x0FFF0000;

[[maybe_unused]] constexpr uint32_t PROP_ID               = PROP_CLASS_SYSTEM     | PROP_TYPE_INTEGER  | 0x0000;
[[maybe_unused]] constexpr uint32_t PROP_DATAFILE         = PROP_CLASS_SYSTEM     | PROP_TYPE_STRING   | 0x0001;
[[maybe_unused]] constexpr uint32_t PROP_CODE             = PROP_CLASS_BOTH       | PROP_TYPE_INTEGER  | 0x0002;
[[maybe_unused]] constexpr uint32_t PROP_DISPLAYNAME      = PROP_CLASS_SYSTEM     | PROP_TYPE_STRING   | 0x0003;
[[maybe_unused]] constexpr uint32_t PROP_ROOTFOLDER       = PROP_CLASS_SYSTEM     | PROP_TYPE_STRING   | 0x0004;
[[maybe_unused]] constexpr uint32_t PROP_LIBUSER          = PROP_CLASS_SYSTEM     | PROP_TYPE_STRING   | 0x0005;
[[maybe_unused]] constexpr uint32_t PROP_LIBPASSWORD      = PROP_CLASS_SYSTEM     | PROP_TYPE_STRING   | 0x0006;
[[maybe_unused]] constexpr uint32_t PROP_URL              = PROP_CLASS_BOTH       | PROP_TYPE_STRING   | 0x0007;
[[maybe_unused]] constexpr uint32_t PROP_CONNECTIONSCRIPT = PROP_CLASS_BOTH       | PROP_TYPE_STRING   | 0x0008;
[[maybe_unused]] constexpr uint32_t PROP_DATAVERSION      = PROP_CLASS_BOTH       | PROP_TYPE_INTEGER  | 0x0009;
[[maybe_unused]] constexpr uint32_t PROP_NOTES            = PROP_CLASS_COLLECTION | PROP_TYPE_STRING   | 0x000A;
[[maybe_unused]] constexpr uint32_t PROP_CREATIONDATE     = PROP_CLASS_COLLECTION | PROP_TYPE_DATETIME | 0x000B;
[[maybe_unused]] constexpr uint32_t PROP_SCHEMA_VERSION   = PROP_CLASS_COLLECTION | PROP_TYPE_STRING   | 0x000C;

[[maybe_unused]] constexpr uint32_t PROP_LAST_AUTHOR      = PROP_CLASS_COLLECTION | PROP_TYPE_STRING   | 0x000D;
[[maybe_unused]] constexpr uint32_t PROP_LAST_AUTHOR_BOOK = PROP_CLASS_COLLECTION | PROP_TYPE_INTEGER  | 0x000F;

[[maybe_unused]] constexpr uint32_t PROP_LAST_SERIES      = PROP_CLASS_COLLECTION | PROP_TYPE_STRING   | 0x0010;
[[maybe_unused]] constexpr uint32_t PROP_LAST_SERIES_BOOK = PROP_CLASS_COLLECTION | PROP_TYPE_INTEGER  | 0x0011;

constexpr uint32_t g_collectionInfoSettings[]
{
	0, 0, PROP_CODE, PROP_NOTES,
};

int g_mhlTriggersOn = 1;

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
		std::wcout << process << " started" << std::endl;
	}
	~Timer()
	{
		std::wcout << process << " done for " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - t).count() << " ms" << std::endl;
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

struct Book
{
	Book(size_t id_
		, std::wstring_view libId_
		, std::wstring_view title_
		, int seriesId_
		, int seriesNum_
		, std::wstring_view date_
		, int rate_
		, std::wstring_view language_
		, std::wstring_view folder_
		, std::wstring_view fileName_
		, size_t insideNo_
		, std::wstring_view format_
		, size_t size_
		, bool isDeleted_
		, std::wstring_view keywords_ = {}
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
		, format(InsertDot(format_))
		, size(size_)
		, isDeleted(isDeleted_)
		, keywords(keywords_)
	{
	}

	size_t      id;
	std::wstring libId;
	std::wstring title;
	int         seriesId;
	int         seriesNum;
	std::wstring date;
	int         rate;
	std::wstring language;
	std::wstring folder;
	std::wstring fileName;
	size_t      insideNo;
	std::wstring format;
	size_t      size;
	bool        isDeleted;
	std::wstring keywords;

private:
	static std::wstring InsertDot(std::wstring_view format)
	{
		std::wstring result(L".");
		result.insert(result.end(), std::cbegin(format), std::cend(format));
		return result;
	}
};

const std::locale g_utf8("ru_RU.UTF-8");

template<typename SizeType, typename StringType>
SizeType StrSize(const StringType & str)
{
	return static_cast<SizeType>(std::size(str));
}
template<typename SizeType>
SizeType StrSize(const char * str)
{
	return static_cast<SizeType>(std::strlen(str));
}
template<typename SizeType>
SizeType StrSize(const wchar_t * str)
{
	return static_cast<SizeType>(std::wcslen(str));
}

template<typename StringType>
const char * MultiByteData(const StringType & str)
{
	return str.data();
}
const char * MultiByteData(const char * data)
{
	return data;
}
template<typename StringType>
std::wstring ToWide(const StringType & str)
{
	const auto size = static_cast<std::wstring::size_type>(MultiByteToWideChar(CP_UTF8, 0, MultiByteData(str), StrSize<int>(str), nullptr, 0));
	std::wstring result(size, 0);
	MultiByteToWideChar(CP_UTF8, 0, MultiByteData(str), StrSize<int>(str), result.data(), StrSize<int>(result));

	return result;
}
template<typename StringType>
const wchar_t * WideData(const StringType & str)
{
	return str.data();
}
const wchar_t * WideData(const wchar_t * data)
{
	return data;
}
template<typename StringType>
std::string ToMultiByte(const StringType & str)
{
	const auto size = static_cast<std::wstring::size_type>(WideCharToMultiByte(CP_UTF8, 0, WideData(str), StrSize<int>(str), nullptr, 0, nullptr, nullptr));
	std::string result(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, WideData(str), StrSize<int>(str), result.data(), StrSize<int>(result), nullptr, nullptr);

	return result;
}

template<typename LhsType, typename RhsType>
bool IsStringEqual(LhsType && lhs, RhsType && rhs)
{
	return CompareStringW(GetThreadLocale(), NORM_IGNORECASE
		, WideData(lhs), StrSize<int>(lhs)
		, WideData(rhs), StrSize<int>(rhs)
	) == CSTR_EQUAL;
}

template<typename LhsType, typename RhsType>
bool IsStringLess(LhsType && lhs, RhsType && rhs)
{
	return CompareStringW(GetThreadLocale(), NORM_IGNORECASE
		, WideData(lhs), StrSize<int>(lhs)
		, WideData(rhs), StrSize<int>(rhs)
	) - CSTR_EQUAL < 0;
}

template <class T = void>
struct StringLess
{
	typedef T _FIRST_ARGUMENT_TYPE_NAME;
	typedef T _SECOND_ARGUMENT_TYPE_NAME;
	typedef bool _RESULT_TYPE_NAME;

	constexpr bool operator()(const T & lhs, const T & rhs) const
	{
		return lhs < rhs;
	}
};
template <>
struct StringLess<void>
{
	template <class LhsType, class RhsType>
	constexpr auto operator()(LhsType && lhs, RhsType && rhs) const
		noexcept(noexcept(static_cast<LhsType &&>(lhs) < static_cast<RhsType &&>(rhs))) // strengthened
		-> decltype(static_cast<LhsType &&>(lhs) < static_cast<RhsType &&>(rhs))
	{
		return IsStringLess(static_cast<LhsType &&>(lhs), static_cast<RhsType &&>(rhs));
	}

	using is_transparent = int;
};

struct Genre
{
	std::wstring code;
	std::wstring parentCore;
	std::wstring name;
	size_t parentId;
	std::wstring dbCode;

	size_t childrenCount{ 0 };

	Genre(std::wstring_view code_, std::wstring_view parentCode_, std::wstring_view name_, size_t parentId_ = 0)
		: code(code_)
		, parentCore(parentCode_)
		, name(name_)
		, parentId(parentId_)
	{
	}
};

using Books = std::vector<Book>;
using Dictionary = std::map<std::wstring, size_t, StringLess<>>;
using Genres = std::vector<Genre>;
using Links = std::vector<std::pair<size_t, size_t>>;
using SettingsTableData = std::map<uint32_t, std::string>;

using GetIdFunctor = std::function<size_t(std::wstring_view)>;
using FindFunctor = std::function<Dictionary::const_iterator(const Dictionary &, std::wstring_view)>;

size_t GetIdDefault(std::wstring_view)
{
	return GetId();
}

Dictionary::const_iterator FindDefault(const Dictionary & container, std::wstring_view value)
{
	return container.find(value);
}

std::ostream & operator<<(std::ostream & stream, const Dictionary::value_type & value)
{
	return stream << value.second << ": " << ToMultiByte(value.first);
}

std::ostream & operator<<(std::ostream & stream, const Links::value_type & value)
{
	return stream << value.first << ": " << value.second;
}

std::ostream & operator<<(std::ostream & stream, const SettingsTableData::value_type & value)
{
	return stream << value.first << ": " << value.second;
}

std::ostream & operator<<(std::ostream & stream, const Genre & value)
{
	return stream << ToMultiByte(value.dbCode) << ", " << ToMultiByte(value.code) << ": " << ToMultiByte(value.name);
}

std::ostream & operator<<(std::ostream & stream, const Book & value)
{
	return stream << ToMultiByte(value.folder) << ", " << value.insideNo << ", " << ToMultiByte(value.libId) << ": " << value.id << ", " << ToMultiByte(value.title);
}

struct Data
{
	Books books;
	Dictionary authors, series;
	Genres genres;
	Links booksAuthors, booksGenres;
	SettingsTableData settings;
};

template <typename It>
std::wstring_view Next(It & beg, const It end, const wchar_t separator)
{
	auto next = std::find(beg, end, separator);
	const std::wstring_view result(beg, next);
	beg = next != end ? std::next(next) : end;
	return result;
}

template<typename T>
T To(std::wstring_view value, T defaultValue = 0)
{
	return value.empty()
		? defaultValue
		: static_cast<T>(_wtoi64(value.data()));
}

class Ini
{
public:
	explicit Ini(const std::filesystem::path & path)
	{
		if (!exists(path))
			throw std::invalid_argument("Need inpx file as command line argument");

		std::wstring line;

		std::wifstream iniStream(path);
		while (std::getline(iniStream, line))
		{
			auto it = std::cbegin(line);
			const auto key = Next(it, std::cend(line), INI_SEPARATOR);
			const auto value = Next(it, std::cend(line), INI_SEPARATOR);
			_data.emplace(key, value);
		}
	}

	const std::wstring & operator()(const wchar_t * key) const
	{
		const auto it = _data.find(key);
		if (it == _data.end())
			throw std::invalid_argument(fmt::format("Cannot find key {}", ToMultiByte(key)));

		return it->second;
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

	index.emplace(L"", std::size(genres));
	auto & root = genres.emplace_back(L"", L"", L"");
	root.dbCode = L"0";

	std::string buf;
	while (std::getline(iniStream, buf))
	{
		const auto line = ToWide(buf);
		auto it = std::cbegin(line);
		const auto codes = Next(it, std::cend(line), GENRE_SEPARATOR);
		auto itCode = std::cbegin(codes);
		std::wstring code;
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

Data Parse(std::wstring_view genresFileName, std::wstring_view inpxFileName, SettingsTableData && settingsTableData)
{
	Timer t(L"parsing archives");

	Data data;
	data.settings = std::move(settingsTableData);

	auto [genresData, genresIndex] = LoadGenres(genresFileName);
	data.genres = std::move(genresData);

	const auto unknownGenreId = genresIndex.find(UNKNOWN)->second;
	auto & unknownGenre = data.genres[unknownGenreId];

	std::vector<std::wstring> unknownGenres;

	const auto archive = ZipFile::Open(ToMultiByte(inpxFileName));
	const auto entriesCount = archive->GetEntriesCount();
	size_t n = 0;

	for (size_t i = 0; i < entriesCount; ++i)
	{
		const auto entry = archive->GetEntry(static_cast<int>(i));
		auto folder = ToWide(entry->GetFullName());
		auto * const stream = entry->GetDecompressionStream();

		if (folder == COLLECTION_INFO)
		{
			ProcessCollectionInfo(*stream, data.settings);
			continue;
		}

		if (folder == VERSION_INFO)
		{
			ProcessVersionInfo(*stream, data.settings);
			continue;
		}

		if (!folder.ends_with(INP_EXT))
		{
			std::wcout << folder << L" skipped" << std::endl;
			continue;
		}

		folder = std::filesystem::path(folder).replace_extension(ZIP).wstring();
		std::wcout << folder << std::endl;

		size_t insideNo = 0;
		std::string buf;
		while (std::getline(*stream, buf))
		{
			const auto id = GetId();

			const auto line = ToWide(buf);
			auto it = std::cbegin(line);
			const auto end = std::cend(line);

			//AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;RATE;KEYWORDS;
			const auto authors    = Next(it, end, FIELDS_SEPARATOR);
			const auto genres     = Next(it, end, FIELDS_SEPARATOR);
			const auto title      = Next(it, end, FIELDS_SEPARATOR);
			const auto seriesName = Next(it, end, FIELDS_SEPARATOR);
			const auto seriesNum  = Next(it, end, FIELDS_SEPARATOR);
			const auto fileName   = Next(it, end, FIELDS_SEPARATOR);
			const auto size       = Next(it, end, FIELDS_SEPARATOR);
			const auto libId      = Next(it, end, FIELDS_SEPARATOR);
			const auto del        = Next(it, end, FIELDS_SEPARATOR);
			const auto ext        = Next(it, end, FIELDS_SEPARATOR);
			const auto date       = Next(it, end, FIELDS_SEPARATOR);
			const auto lang       = Next(it, end, FIELDS_SEPARATOR);
			const auto rate       = Next(it, end, FIELDS_SEPARATOR);
//			const auto keywords   = Next(it, end, FIELDS_SEPARATOR);

			ParseItem(id, authors, data.authors, data.booksAuthors);
			ParseItem(id, genres, genresIndex, data.booksGenres
				, [unknownGenreId, &unknownGenre, &unknownGenres, &data = data.genres](std::wstring_view title)
					{
						const auto result = std::size(data);
						auto & genre = data.emplace_back(title, L"", title, unknownGenreId);
						genre.dbCode = ToWide(fmt::format("{0}.{1}", ToMultiByte(unknownGenre.dbCode), ++unknownGenre.childrenCount));
						unknownGenres.push_back(genre.name);
						return result;
					}
				, [&data = data.genres](const Dictionary & container, std::wstring_view value)
					{
						const auto it = container.find(value);
						return it != container.end() ? it : std::ranges::find_if(container, [value, &data](const auto & item)
						{
							return IsStringEqual(value, data[item.second].name);
						});
					}
			);

			data.books.emplace_back(id, libId, title, Add<int, -1>(seriesName, data.series), To<int>(seriesNum, -1), date, To<int>(rate), lang, folder, fileName, insideNo++, ext, To<size_t>(size), To<bool>(del, false)/*, keywords*/);

			if ((++n % LOG_INTERVAL) == 0)
				std::cout << n << " rows parsed" << std::endl;
		}
	}

	std::cout << n << " rows parsed" << std::endl;

	if (!std::empty(unknownGenres))
	{
		std::cerr << "Unknown genres" << std::endl;
		std::transform(std::cbegin(unknownGenres), std::cend(unknownGenres), std::ostream_iterator<std::string>(std::cerr, "\n"), &ToMultiByte<std::wstring>);
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

SettingsTableData ReadSettings(const std::wstring & dbFileName)
{
	SettingsTableData data;

	sqlite3pp::database db(ToMultiByte(dbFileName).data());
	if (!TableExists(db, "Settings"))
		return {};

	sqlite3pp::query query(db, "select SettingID, SettingValue from Settings");

	std::transform(std::begin(query), std::end(query), std::inserter(data, std::end(data)), [](const auto & row)
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

void ReCreateDatabase(std::wstring_view dbFileName, std::wstring_view createDbScript)
{
	std::cout << "Recreating database" << std::endl;

	char stubExe[] = "stub.exe";
	auto readScript = fmt::format(".read {}.", ToMultiByte(createDbScript));
	auto loadExt = fmt::format(".load {}", MHL_SQLITE_EXTENSION);
	auto dbFileNameStr = ToMultiByte(dbFileName);

	char * v[]
	{
		stubExe,
		dbFileNameStr.data(),
		loadExt.data(),
		readScript.data(),
	};

	if (SQLiteShellExecute(static_cast<int>(std::size(v)), v) != 0)
		throw std::runtime_error("Cannot create database");
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
		std::cout << fmt::format("{0} rows inserted ({1}%)", rowsInserted, rowsInserted * 100 / rowsTotal) << std::endl;
	};

	const auto result = std::accumulate(beg, end, size_t{ 0 }, [f = std::forward<Functor>(f), &db, &cmd, &rowsInserted, &log](size_t init, const typename It::value_type & value)
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
			std::cerr << db.error_code() << ": " << db.error_msg() << std::endl << value << std::endl;
		}

		return init + result;
	});

	log();
	if (rowsTotal != rowsInserted)
		std::cerr << rowsTotal - rowsInserted << " rows lost" << std::endl;

	tr.commit();

	return result;
}

size_t Store(std::wstring_view dbFileName, const Data & data)
{
	size_t result = 0;
	result += StoreRange(dbFileName, "Authors", "INSERT INTO Authors (AuthorID, LastName, FirstName, MiddleName) VALUES(?, ?, ?, ?)", std::cbegin(data.authors), std::cend(data.authors), [](sqlite3pp::command & cmd, const Dictionary::value_type & item)
	{
		const auto & [author, id] = item;
		auto it = std::cbegin(author);
		const auto last   = Next(it, std::cend(author), NAMES_SEPARATOR);
		const auto first  = Next(it, std::cend(author), NAMES_SEPARATOR);
		const auto middle = Next(it, std::cend(author), NAMES_SEPARATOR);
		cmd.binder() << id << ToMultiByte(last) << ToMultiByte(first) << ToMultiByte(middle);
	});

	result += StoreRange(dbFileName, "Series", "INSERT INTO Series (SeriesID, SeriesTitle) VALUES (?, ?)", std::cbegin(data.series), std::cend(data.series), [](sqlite3pp::command & cmd, const Dictionary::value_type & item)
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
	result += StoreRange(dbFileName, "Books", queryText, std::cbegin(data.books), std::cend(data.books), [](sqlite3pp::command & cmd, const Book & book)
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

	result += StoreRange(dbFileName, "Author_List", "INSERT INTO Author_List (AuthorID, BookID) VALUES(?, ?)", std::cbegin(data.booksAuthors), std::cend(data.booksAuthors), [](sqlite3pp::command & cmd, const Links::value_type & item)
	{
		cmd.binder() << item.second << item.first;
	}, false);

	result += StoreRange(dbFileName, "Genre_List", "INSERT INTO Genre_List (BookID, GenreCode) VALUES(?, ?)", std::cbegin(data.booksGenres), std::cend(data.booksGenres), [&genres = data.genres](sqlite3pp::command & cmd, const Links::value_type & item)
	{
		assert(item.second < std::size(genres));
		cmd.binder() << item.first << ToMultiByte(genres[item.second].dbCode);
	}, false);

	result += StoreRange(dbFileName, "Settings", "INSERT INTO Settings (SettingID, SettingValue) VALUES (?, ?)", std::cbegin(data.settings), std::cend(data.settings), [](sqlite3pp::command & cmd, const SettingsTableData::value_type & item)
	{
		cmd.binder() << static_cast<size_t>(item.first) << item.second;
	}, false);

	return result;
}

}

void mainImpl(int argc, char * argv[])
{
	const auto ini = ParseConfig(argc, argv);

	g_mhlTriggersOn = _wtoi(ini(MHL_TRIGGERS_ON, std::to_wstring(g_mhlTriggersOn)).data());
	const auto dbFileName = ini(DB_PATH);

	auto settingsTableData = ReadSettings(dbFileName);
	ReCreateDatabase(dbFileName, ini(CREATE_DB_SCRIPT));

	const auto data = Parse(ini(GENRES), ini(INPX), std::move(settingsTableData));
	if (const auto failsCount = Store(dbFileName, data); failsCount != 0)
		std::cerr << "Something went wrong" << std::endl;
}

int main(int argc, char* argv[])
{
	try
	{
		Timer t(L"work");
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
