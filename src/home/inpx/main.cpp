__pragma(warning(push, 0))

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <unordered_map>

#include "ZipLib/ZipFile.h"
#include "sqlite/shell/sqlite_shell.h"
#include "sqlite3pp.h"

#include <Windows.h>

__pragma(warning(pop))

namespace {

constexpr std::string_view INP_EXT = ".inp";
constexpr char FIELDS_SEPARATOR = '\x04';
constexpr char INI_SEPARATOR = '=';
constexpr char LIST_SEPARATOR = ':';
constexpr char NAMES_SEPARATOR = ',';
constexpr char GENRE_SEPARATOR = '|';

constexpr const char * GENRES           = "genres";
constexpr const char * INI_EXT          = "ini";
constexpr const char * INPX             = "inpx";
constexpr const char * UNKNOWN          = "unknown";

constexpr int64_t LOG_INTERVAL = 10000;

int64_t g_id = 0;
int64_t GetId()
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
	Book(int64_t id_
		, std::string_view title_
		, int64_t seriesId_
		, int64_t seriesNum_
		, std::string_view archive_
		, std::string_view file_
		, int64_t size_
		, std::string_view format_
		, std::string_view date_
		, std::string_view language_
		, int64_t rate_
		, std::string_view keywords_
	)
		: id(id_)
		, title(title_)
		, seriesId(seriesId_)
		, seriesNum(seriesNum_)
		, archive(archive_)
		, file(file_)
		, size(size_)
		, format(format_)
		, date(date_)
		, language(language_)
		, rate(rate_)
		, keywords(keywords_)
	{
	}

	int64_t     id;
	std::string title;
	int64_t     seriesId;
	int64_t     seriesNum;
	std::string archive;
	std::string file;
	int64_t     size;
	std::string format;
	std::string date;
	std::string language;
	int64_t rate;
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
	int64_t id;
	std::string code;
	std::string parentCore;
	std::string name;
	int64_t parentId;

	std::string nameLower;

	Genre(int64_t id_, std::string_view code_, std::string_view parentCode_, std::string_view name_, int64_t parentId_ = 0)
		: id(id_)
		, code(code_)
		, parentCore(parentCode_)
		, name(name_)
		, parentId(parentId_)
		, nameLower(toLower(name))
	{
		std::cout << nameLower << std::endl;
	}
};
	
using Books = std::vector<Book>;
using Dictionary = std::unordered_map<std::string, int64_t, StringHash, std::equal_to<>>;
using Genres = std::vector<Genre>;
using Links = std::vector<std::pair<int64_t, int64_t>>;

using GetIdFunctor = std::function<int64_t(std::string_view)>;
using FindFunctor = std::function<Dictionary::iterator(Dictionary &, std::string_view)>;

int64_t GetIdDefault(std::string_view)
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

	const std::string & Get(const char * key) const
	{
		const auto it = _data.find(key);
		if (it == _data.end())
			throw std::invalid_argument(std::string("Cannot find key ") + key);

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
		throw std::invalid_argument(std::string("Cannot open '") + genresIniFileName.data() + "'");
	
	std::string line;
	while (std::getline(iniStream, line))
	{
		auto it = std::cbegin(line);
		const auto codes   = Next(it, std::cend(line), GENRE_SEPARATOR);
		auto itCode = std::cbegin(codes);
		std::string code;
		while(itCode != std::cend(codes))
		{
			const auto & added = index.emplace(Next(itCode, std::cend(codes), LIST_SEPARATOR), static_cast<int64_t>(std::size(genres))).first->first;
			if (code.empty())
				code = added;
		}
		
		const auto parent = Next(it, std::cend(line), GENRE_SEPARATOR);
		const auto title  = Next(it, std::cend(line), GENRE_SEPARATOR);
		genres.emplace_back(GetId(), code, parent, title);
	}

	for (auto & genre : genres)
	{
		if (genre.parentCore.empty())
			continue;

		const auto it = index.find(genre.parentCore);
		if (it != index.end())
			genre.parentId = genres[static_cast<size_t>(it->second)].id;
	}

	return std::make_pair(std::move(genres), std::move(index));
}

int64_t Add(std::string_view value, Dictionary & container, const GetIdFunctor & getId = &GetIdDefault, const FindFunctor & find = &FindDefault)
{
	if (value.empty())
		return 0;

	auto it = find(container, value);
	if (it == container.end())
		it = container.emplace(value, getId(value)).first;

	return it->second;
}

void ParseItem(int64_t id, std::string_view data, Dictionary & container, Links & links, const GetIdFunctor & getId = &GetIdDefault, const FindFunctor & find = &FindDefault)
{
	auto it = std::cbegin(data);
	while (it != std::cend(data))
	{
		const auto value = Next(it, std::cend(data), LIST_SEPARATOR);
		links.emplace_back(id, Add(value, container, getId, find));
	}
}

Data Parse(std::string_view genresName, std::string_view inpxFileName)
{
	Timer t("parsing archives");

	Data data;
	auto [genresData, genresIndex] = LoadGenres(genresName);
	data.genres = std::move(genresData);
	const auto unknownGenreId = data.genres[static_cast<size_t>(genresIndex.find(UNKNOWN)->second)].id;

	std::vector<std::string> unknownGenres;
	
	const auto archive = ZipFile::Open(inpxFileName.data());
	const auto entriesCount = archive->GetEntriesCount();
	int64_t n = 0;

	for (size_t i = 0; i < entriesCount; ++i)
	{
		const auto entry = archive->GetEntry(static_cast<int>(i));
		const auto filename = entry->GetFullName();

		if (!filename.ends_with(INP_EXT))
		{
			std::cout << filename << " skipped" << std::endl;
			continue;
		}

		std::cout << filename << std::endl;

		auto * const stream = entry->GetDecompressionStream();
		assert(stream);

		std::string line;
		while (std::getline(*stream, line))
		{
			const auto id = GetId();

			auto itBook = std::cbegin(line);
			//AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;RATE;KEYWORDS;
			const auto authors    =    Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto genres     =    Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto title      =    Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto seriesName =    Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto seriesNum  =    Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto file       =    Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto size       =    Next(itBook, std::cend(line), FIELDS_SEPARATOR);
/*			const auto libid      = */ Next(itBook, std::cend(line), FIELDS_SEPARATOR);
/*			const auto del        = */ Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto ext        =    Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto date       =    Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto lang       =    Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto rate       =    Next(itBook, std::cend(line), FIELDS_SEPARATOR);
			const auto keywords   =    Next(itBook, std::cend(line), FIELDS_SEPARATOR);

			ParseItem(id, authors, data.authors, data.booksAuthors);
			ParseItem(id, genres, genresIndex, data.booksGenres
				, [unknownGenreId, &unknownGenres, &data = data.genres](std::string_view title)
					{
						const auto result = static_cast<int64_t>(std::size(data));
						data.emplace_back(GetId(), title, "", title, unknownGenreId);
						unknownGenres.push_back(data.back().nameLower);
						return result;
					}
				, [&data = data.genres](Dictionary & container, std::string_view value)
					{
						const auto it = container.find(value);
						const auto v = toLower(std::string(std::cbegin(value), std::cend(value)));
						return it != container.end() ? it : std::ranges::find_if(container, [&data, &v](const Dictionary::value_type & item)
						{
							return v == data[static_cast<size_t>(item.second)].nameLower;
						});
					}
			);

			data.books.emplace_back(id, title, Add(seriesName, data.series), To<int64_t>(seriesNum, -1), filename, file, To<int64_t>(size), ext, date, lang, To<int64_t>(rate, -1), keywords);

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

}

void mainImpl(int argc, char * argv[])
{
	std::ifstream t("D:/src/other/home/books/build64/bin/CreateCollectionDB_SQLite.sql");
	std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

	char v1[] = "exe";
	char v2[] = R"(D:\src\other\home\books\build64\bin\base.db)";
	char v3[] = ".read D:/src/other/home/books/build64/bin/CreateCollectionDB_SQLite.sql";
	
	char * v[]
	{
		v1,
		v2,
		v3,
	};
	
	SQLiteShellExecute(static_cast<int>(std::size(v)), v);
	
	sqlite3pp::database db("test.db");
	sqlite3pp::transaction tr(db);
	[[maybe_unused]] auto res = sqlite3pp::command(db, str.data()).execute_all();
	tr.commit();

	const auto ini = ParseConfig(argc, argv);
	const auto data = Parse(ini.Get(GENRES), ini.Get(INPX));
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
