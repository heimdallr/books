#pragma warning(push, 0)

#include <filesystem>
#include <fstream>
#include <future>
#include <queue>
#include <ranges>
#include <set>

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include <plog/Log.h>

#include "sqlite3ppext.h"

#pragma warning(pop)

#include "constant.h"
#include "types.h"

#include "inpx.h"

#include "fnd/IsOneOf.h"
#include "util/executor/factory.h"
#include "util/IExecutor.h"
#include "util/timer.h"

#include "zip.h"

#include "Fb2Parser.h"

using namespace HomeCompa;
using namespace Inpx;

namespace {

using Path = Parser::IniMap::value_type::second_type;

size_t g_id = 0;
size_t GetId()
{
	return ++g_id;
}

size_t GetIdDefault(std::wstring_view)
{
	return GetId();
}

bool ParseCheckerDefault(std::wstring_view)
{
	return true;
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
	explicit Ini(Parser::IniMap data)
		: _data(std::move(data))
	{
	}

	explicit Ini(const Path & path)
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
			_data.try_emplace(std::wstring(key), value);
		}
	}

	const Path & operator()(const Parser::IniMap::value_type::first_type & key, const Path & defaultValue = {}) const
	{
		const auto it = _data.find(key);
		if (it != _data.end())
			return it->second;

		if (!defaultValue.empty())
			return defaultValue;

		assert(false && "value required");
		throw std::runtime_error("value required");
	}

private:
	Parser::IniMap _data;
};

class DatabaseWrapper
{
public:
	explicit DatabaseWrapper(const Path & dbFileName, const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
		: m_db(QString::fromStdWString(dbFileName).toUtf8(), flags)
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

auto LoadGenres(const Path & genresIniFileName)
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

std::set<size_t> ParseItem(const std::wstring_view data
	, Dictionary & container
	, const wchar_t separator = LIST_SEPARATOR
	, const ParseChecker & parseChecker = &ParseCheckerDefault
	, const GetIdFunctor & getId = &GetIdDefault
	, const FindFunctor & find = &FindDefault
)
{
	std::set<size_t> result;
	auto it = std::cbegin(data);
	while (it != std::cend(data))
	{
		if (const auto value = Next(it, std::cend(data), separator); parseChecker(value))
			result.emplace(Add<size_t>(value, container, getId, find));
	}
	return result;
}

std::set<size_t> ParseItem(const std::wstring_view data
	, Dictionary & container
	, const Splitter & splitter
	, const GetIdFunctor & getId = &GetIdDefault
	, const FindFunctor & find = &FindDefault
)
{
	std::set<size_t> result;
	std::ranges::transform(splitter(data), std::inserter(result, result.end()), [&] (const auto & value)
	{
		return Add<size_t>(value, container, getId, find);
	});
	return result;
}

std::set<size_t> ParseKeywords(std::wstring_view keywordsSrc, Dictionary& keywordsLinks, std::unordered_map<QString, std::wstring>& uniqueKeywords)
{
	return ParseItem(keywordsSrc, keywordsLinks
		, [&](const std::wstring_view item)
		{
			QStringList keywordsList;
			QString keywordsStr = QString::fromWCharArray(item.data(), static_cast<int>(item.size()))
				.replace("--", ",")
				.replace(" - ", ",")
				.replace(" & ", " and ")
				.replace(QChar(L'\x0401'), QChar(L'\x0415'))
				.replace(QChar(L'\x0451'), QChar(L'\x0435'))
				.replace("_", " ")
				;
			keywordsStr.remove(QRegularExpression(QString::fromStdWString(L"[@!\\?\"\x00ab\x00bb]")));
			auto list = keywordsStr.split(QRegularExpression(R"([,;#/\\\.\(\)\[\]])"), Qt::SkipEmptyParts);
			std::ranges::transform(list, list.begin(), [](const auto& str) { return str.simplified(); });

			if (const auto [begin, end] = std::ranges::remove_if(list, [](const auto& str) { return str.length() < 3 || str.startsWith("DocId:", Qt::CaseInsensitive); }); begin != end)
				list.erase(begin, end);
			assert(std::ranges::none_of(list, [](const auto& str) { return str.startsWith("DocId:", Qt::CaseInsensitive); }));

			for (auto& keyword : list)
				keywordsList << keyword.split(':', Qt::SkipEmptyParts);

			for (auto& keyword : keywordsList)
			{
				if (const auto it = std::ranges::find_if(keyword, [](const QChar& c) { return c.isLetterOrNumber() || IsOneOf(c, '+'); }); it != keyword.begin())
					keyword = keyword.last(std::distance(it, keyword.end()));
				keyword = keyword.simplified();
				if (!keyword.isEmpty())
					keyword[0] = keyword[0].toUpper();
			}
			if (const auto [begin, end] = std::ranges::remove_if(keywordsList, [](const auto& str) { return str.length() < 3; }); begin != end)
				keywordsList.erase(begin, end);

			std::vector<std::wstring> result;
			result.reserve(keywordsList.size());
			std::ranges::transform(keywordsList, std::back_inserter(result), [](const auto& str) { return str.toStdWString(); });
			return result;
		}
		, &GetIdDefault
		, [&](const Dictionary& container, const std::wstring_view value)
		{
			auto key = QString::fromWCharArray(value.data(), static_cast<int>(value.size())).toLower();
			if (const auto it = uniqueKeywords.find(key); it != uniqueKeywords.end())
				return container.find(it->second);

			uniqueKeywords.try_emplace(std::move(key), value);
			return container.end();
		});
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
		genre.dateGenre = true;
		return itGenre;
	};

	if (buf.DATE.empty())
	{
		auto itIndex = genresIndex.find(NO_DATE_SPECIFIED);
		if (itIndex == genresIndex.end())
			itIndex = addGenre(L"no_date_specified", NO_DATE_SPECIFIED, genresIndex.find(DATE_ADDED_CODE));
		idGenres.emplace(itIndex->second);
		return;
	}

	auto itDate = std::cbegin(buf.DATE);
	const auto endDate = std::cend(buf.DATE);
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

using BookBufFieldGetter = std::wstring_view& (*)(BookBuf&);
using BookBufMapping = std::vector<BookBufFieldGetter>;

#define BOOK_BUF_FIELD_ITEM(NAME) std::wstring_view& Get##NAME(BookBuf& buf) { return buf.NAME; }
		BOOK_BUF_FIELD_ITEMS_XMACRO
#undef	BOOK_BUF_FIELD_ITEM

BookBuf ParseBook(std::wstring_view folder, std::wstring & line, const BookBufMapping& f)
{
	BookBuf buf{ .FOLDER = folder };

	auto it = std::begin(line);
	const auto end = std::end(line);
	for (size_t i = 0, sz = f.size(); i < sz && it != end; ++i)
		f[i](buf) = Next(it, end, FIELDS_SEPARATOR);

	return buf;
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

InpxContent ExtractInpxFileNames(const Path & inpxFileName)
{
	if (!exists(inpxFileName))
		return {};

	InpxContent inpxContent;

	const Zip zip(QString::fromStdWString(inpxFileName.generic_wstring()));
	for (const auto & fileName : zip.GetFileNameList())
	{
		auto folder = ToWide(fileName.toStdString());

		if (folder == COLLECTION_INFO)
			inpxContent.collectionInfo.emplace_back(std::move(folder));
		else if (folder == VERSION_INFO)
			inpxContent.versionInfo.emplace_back(std::move(folder));
		else if (folder.ends_with(INP_EXT))
			inpxContent.inpx.emplace_back(std::move(folder));
		else
			PLOGI << folder << L" skipped";
	}

	return inpxContent;
}

void GetDecodedStream(const Zip & zip, const std::wstring & file, const std::function<void(QIODevice&)> & f)
{
	PLOGI << file;
	try
	{
		const auto stream = zip.Read(QString::fromStdWString(file));
		f(stream->GetStream());
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

void ExecuteScript(const std::wstring & action, const Path & dbFileName, const Path & scriptFileName)
{
	Util::Timer t(action);

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

void Analyze(const Path & dbFileName)
{
	DatabaseWrapper db(dbFileName);
	PLOGI << "ANALYZE";
	[[maybe_unused]] const auto rc = sqlite3pp::command(db, "ANALYZE").execute();
	assert(rc == 0);
}

template<typename Container, typename Functor>
size_t StoreRange(const Path & dbFileName, std::string_view process, const std::string_view query, const Container& container, Functor && f)
{
	const auto rowsTotal = std::size(container);
	if (rowsTotal == 0)
		return rowsTotal;

	Util::Timer t(ToWide(std::format("store {0} {1}", process, rowsTotal)));
	size_t rowsInserted = 0;

	DatabaseWrapper db(dbFileName);
	sqlite3pp::transaction tr(db);
	sqlite3pp::command cmd(db, query.data());

	const auto log = [rowsTotal, &rowsInserted]
	{
		PLOGD << std::format("{0} rows inserted ({1}%)", rowsInserted, rowsInserted * 100 / rowsTotal);
	};

	const auto result = std::accumulate(std::cbegin(container), std::cend(container), static_cast<size_t>(0), [f = std::forward<Functor>(f), &db, &cmd, &rowsInserted, &log] (const size_t init, const auto & value)
	{
		const auto localResult = f(cmd, value) + cmd.reset();

		if (localResult == 0)
		{
			if (++rowsInserted % LOG_INTERVAL == 0)
				log();
		}
		else
		{
			PLOGE << db->error_code() << ": " << db->error_msg() << std::endl << value;
		}

		return init + static_cast<size_t>(localResult);
	});

	log();
	if (rowsTotal != rowsInserted)
		PLOGE << rowsTotal - rowsInserted << " rows lost";
	{
		Util::Timer tc(L"commit");
		tr.commit();
	}

	return result;
}

size_t Store(const Path & dbFileName, const Data & data)
{
	size_t result = 0;
	result += StoreRange(dbFileName, "Authors", "INSERT INTO Authors (AuthorID, LastName, FirstName, MiddleName, SearchName) VALUES(?, ?, ?, ?, MHL_UPPER(?))", data.authors, [] (sqlite3pp::command & cmd, const Dictionary::value_type & item)
	{
		const auto & [author, id] = item;
		auto it = std::cbegin(author);
		const auto last   = ToMultiByte(Next(it, std::cend(author), NAMES_SEPARATOR));
		const auto first  = ToMultiByte(Next(it, std::cend(author), NAMES_SEPARATOR));
		const auto middle = ToMultiByte(Next(it, std::cend(author), NAMES_SEPARATOR));

		cmd.bind(1, id);
		cmd.bind(2, last, sqlite3pp::nocopy);
		cmd.bind(3, first, sqlite3pp::nocopy);
		cmd.bind(4, middle, sqlite3pp::nocopy);
		cmd.bind(5, last, sqlite3pp::nocopy);

		return cmd.execute();
	});

	result += StoreRange(dbFileName, "Series", "INSERT INTO Series (SeriesID, SeriesTitle, SearchTitle) VALUES (?, ?, MHL_UPPER(?))", data.series, [] (sqlite3pp::command & cmd, const Dictionary::value_type & item)
	{
		const auto title = ToMultiByte(item.first);
		cmd.bind(1, item.second);
		cmd.bind(2, title, sqlite3pp::nocopy);
		cmd.bind(3, title, sqlite3pp::nocopy);
		return cmd.execute();
	});

	result += StoreRange(dbFileName, "Folders", "INSERT INTO Folders (FolderID, FolderTitle) VALUES (?, ?)", data.folders, [] (sqlite3pp::command & cmd, const auto & item)
	{
		const auto title = ToMultiByte(item.first);
		cmd.bind(1, item.second);
		cmd.bind(2, title, sqlite3pp::nocopy);
		return cmd.execute();
	});

	result += StoreRange(dbFileName, "Keywords", "INSERT INTO Keywords (KeywordID, KeywordTitle, SearchTitle) VALUES (?, ?, MHL_UPPER(?))", data.keywords, [] (sqlite3pp::command & cmd, const Dictionary::value_type & item)
	{
		const auto title = ToMultiByte(item.first);
		cmd.bind(1, item.second);
		cmd.bind(2, title, sqlite3pp::nocopy);
		cmd.bind(3, title, sqlite3pp::nocopy);
		return cmd.execute();
	});

	std::vector<size_t> newGenresIndex;
	for (size_t i = 0, sz = std::size(data.genres); i < sz; ++i)
		if (data.genres[i].newGenre)
			newGenresIndex.push_back(i);
	result += StoreRange(dbFileName, "Genres", "INSERT INTO Genres (GenreCode, ParentCode, FB2Code, GenreAlias) VALUES(?, ?, ?, ?)", newGenresIndex | std::views::drop(1), [&genres = data.genres](sqlite3pp::command & cmd, const size_t n)
	{
		cmd.binder() << ToMultiByte(genres[n].dbCode) << ToMultiByte(genres[genres[n].parentId].dbCode) << ToMultiByte(genres[n].code) << ToMultiByte(genres[n].name);
		return cmd.execute();
	});

	const char * queryText = "INSERT INTO Books ("
		"BookID   , LibID     , Title    , SeriesID, "
		"SeqNumber, UpdateDate, LibRate  , Lang    , "
		"FolderID , FileName  , InsideNo , Ext     , "
		"BookSize , IsDeleted, SearchTitle"
		") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, MHL_UPPER(?))";
	result += StoreRange(dbFileName, "Books", queryText, data.books, [] (sqlite3pp::command & cmd, const Book & book)
	{
		const auto libId = ToMultiByte(book.libId);
		const auto title = ToMultiByte(book.title);
		const auto date = ToMultiByte(book.date);
		const auto language = ToMultiByte(book.language);
		const auto fileName = ToMultiByte(book.fileName);
		const auto format = ToMultiByte(book.format);
																		   cmd.bind( 1, book.id);
																		   cmd.bind( 2, libId, sqlite3pp::nocopy);
																		   cmd.bind( 3, title, sqlite3pp::nocopy);
			book.seriesId  == -1  ? cmd.bind( 4, sqlite3pp::null_type()) : cmd.bind( 4, book.seriesId);
			book.seriesNum == -1  ? cmd.bind( 5, sqlite3pp::null_type()) : cmd.bind( 5, book.seriesNum);
																		   cmd.bind( 6, date, sqlite3pp::nocopy);
																		   cmd.bind( 7, book.rate);
																		   cmd.bind( 8, language, sqlite3pp::nocopy);
																		   cmd.bind( 9, book.folder);
																		   cmd.bind(10, fileName, sqlite3pp::nocopy);
																		   cmd.bind(11, book.insideNo);
																		   cmd.bind(12, format, sqlite3pp::nocopy);
																		   cmd.bind(13, book.size);
																		   cmd.bind(14, book.isDeleted ? 1 : 0);
																		   cmd.bind(15, title, sqlite3pp::nocopy);
																		   return cmd.execute();
	});

	result += StoreRange(dbFileName, "Author_List", "INSERT INTO Author_List (AuthorID, BookID) VALUES(?, ?)", data.booksAuthors, [] (sqlite3pp::command & cmd, const Links::value_type & item)
	{
		cmd.binder() << item.second << item.first;
		return cmd.execute();
	});

	result += StoreRange(dbFileName, "Keyword_List", "INSERT INTO Keyword_List (KeywordID, BookID) VALUES(?, ?)", data.booksKeywords, [] (sqlite3pp::command & cmd, const Links::value_type & item)
	{
		cmd.binder() << item.second << item.first;
		return cmd.execute();
	});
	std::vector<std::string> genres;
	genres.reserve(std::size(genres));
	std::ranges::transform(data.genres, std::back_inserter(genres), [] (const auto & item) { return ToMultiByte(item.dbCode); });
	result += StoreRange(dbFileName, "Genre_List", "INSERT INTO Genre_List (BookID, GenreCode) VALUES(?, ?)", data.booksGenres, [genres = std::move(genres)] (sqlite3pp::command & cmd, const Links::value_type & item)
	{
		assert(item.second < std::size(genres));
		cmd.bind(1, item.first);
		cmd.bind(2, genres[item.second], sqlite3pp::nocopy);
		return cmd.execute();
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

auto GetInpxFilesInFolder(const Path & inpxFolder)
{
	return std::filesystem::directory_iterator { inpxFolder } | std::views::filter([] (const Path & path)
	{
		const auto ext = path.extension().wstring();
		const auto proj = [] (const auto & ch) { return std::tolower(ch); };
		return std::ranges::equal(ext, std::wstring(INPX_EXT), {}, proj, proj);
	});
}

auto GetNewInpxFolders(const Ini & ini, Data & data)
{
	std::map<std::wstring, std::wstring, CaseInsensitiveComparer<>> dbExt;
	std::ranges::transform(data.folders, std::inserter(dbExt, std::end(dbExt)), [] (const auto & item)
	{
		auto folder = item.first;
		auto ext = RemoveExt(folder);
		return std::make_pair(std::move(folder), std::move(ext));
	});
	std::map<std::wstring, std::wstring, CaseInsensitiveComparer<>> inpxExt;
	const auto inpxFolder = ini(INPX_FOLDER);
	std::unordered_map<Path, std::vector<std::wstring>, CaseInsensitiveHash<Path>> result;

	for (const auto & inpx : GetInpxFilesInFolder(inpxFolder))
	{
		std::ranges::transform(ExtractInpxFileNames(inpx).inpx, std::inserter(inpxExt, std::end(inpxExt)), [] (auto item)
		{
			auto ext = RemoveExt(item);
			return std::make_pair(std::move(item), std::move(ext));
		});

		std::vector<std::pair<std::wstring, std::wstring>> resultExt;
		const auto proj = [] (const auto & item) { return item.first; };
		std::ranges::set_difference(inpxExt, dbExt, std::back_inserter(resultExt), {}, proj, proj);
		if (resultExt.empty())
			continue;

		auto & files = result.try_emplace(inpx, std::vector<std::wstring>{}).first->second;
		files.reserve(resultExt.size());
		std::ranges::transform(resultExt, std::back_inserter(files), [&] (const auto & item) { return item.first + L'.' + item.second; });
	}

	return result;
}

template<typename T>
T ReadDictionary(const std::string_view name, sqlite3pp::database & db, const char * statement)
{
	PLOGI << "Read " << name;
	T data;
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

std::pair<Genres, Dictionary> ReadGenres(sqlite3pp::database & db, const Path & genresFileName)
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
	sqlite3pp::query query(db, GET_MAX_ID_QUERY);
	g_id = (*query.begin()).get<long long>(0);
	PLOGD << "Next Id: " << g_id;
}

std::pair<Data, Dictionary> ReadData(const Path & dbFileName, const Path & genresFileName)
{
	Data data;

	DatabaseWrapper db(dbFileName, SQLITE_OPEN_READONLY);
	SetNextId(db);
	data.authors = ReadDictionary<Dictionary>("authors", db, "select AuthorID, LastName||','||FirstName||','||MiddleName from Authors");
	data.series = ReadDictionary<Dictionary>("series", db, "select SeriesID, SeriesTitle from Series");
	data.keywords = ReadDictionary<Dictionary>("keywords", db, "select KeywordID, KeywordTitle from Keywords");
	data.folders = ReadDictionary<Folders>("folders", db, "select FolderID, FolderTitle from Folders");
	auto [genres, genresIndex] = ReadGenres(db, genresFileName);
	data.genres = std::move(genres);

	return std::make_pair(std::move(data), std::move(genresIndex));
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
			const auto genres = static_cast<size_t>(std::ranges::count_if(m_data.genres, [] (const Genre & genre) { return genre.newGenre && !genre.dateGenre; })) - 1;
			return [this, genres] (size_t)
			{
				m_callback(UpdateResult { m_data.folders.size(), m_data.authors.size(), m_data.series.size(), m_data.books.size(), m_data.keywords.size(), genres });
			};
		} });
	}

	void UpdateDatabase()
	{
		(*m_executor)({ "Update collection", [&]
		{
			const auto foldersCount = UpdateDatabaseImpl();
			const auto genres = static_cast<size_t>(std::ranges::count_if(m_data.genres, [] (const Genre & genre) { return genre.newGenre && !genre.dateGenre; })) - 1;
			return [this, foldersCount, genres] (size_t)
			{
				m_callback(UpdateResult { foldersCount, m_data.authors.size(), m_data.series.size(), m_data.books.size(), m_data.keywords.size(), genres });
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
				PLOGI << "parsing " << folder;

				const QFileInfo archiveFileInfo(QString::fromStdWString(m_rootFolder / folder));
				const Zip zip(archiveFileInfo.filePath());
				const auto zipFileList = zip.GetFileNameList();
				for (size_t counter = 0; const auto & fileName : zipFileList)
				{
					++counter;
					if (QFileInfo(fileName).suffix() == "fb2")
					{
						try
						{
							PLOGV << "parsing " << folder << "/" << fileName << "  " << counter << " (" << zipFileList.size() << ") " << 100 * counter / zipFileList.size() << "%";
							ParseFile(folder, zip, fileName, archiveFileInfo.birthTime());
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
		Util::Timer t(L"work");

		const auto & dbFileName = m_ini(DB_PATH);

		ExecuteScript(L"create database", dbFileName, m_ini(DB_CREATE_SCRIPT, DEFAULT_DB_CREATE_SCRIPT));

		Parse();
		if (const auto failsCount = Store(dbFileName, m_data); failsCount != 0)
			PLOGE << "Something went wrong";

		ExecuteScript(L"update database", dbFileName, m_ini(DB_UPDATE_SCRIPT, DEFAULT_DB_UPDATE_SCRIPT));
	}

	void SetUnknownGenreId()
	{
		static constexpr auto UNKNOWN = L"unknown_root";
		const auto it = m_genresIndex.find(UNKNOWN);
		assert(it != m_genresIndex.end());
		m_unknownGenreId = it->second;
	}

	size_t UpdateDatabaseImpl()
	{
		const auto & dbFileName = m_ini(DB_PATH);
		const auto [oldData, oldGenresIndex] = ReadData(dbFileName, m_ini(GENRES, DEFAULT_GENRES));

		std::ranges::transform(oldData.keywords, std::inserter(m_uniqueKeywords, m_uniqueKeywords.end()), [] (const auto & item) { return std::pair(QString::fromStdWString(item.first).toLower(), item.first); });

		m_data = oldData;
		m_genresIndex = oldGenresIndex;
		SetUnknownGenreId();

		size_t result = 0;
		const auto newInpxFolders = GetNewInpxFolders(m_ini, m_data);
		for (const auto & inpxFileNameEntry : GetInpxFilesInFolder(m_ini(INPX_FOLDER)))
		{
			const auto & inpxFileName = inpxFileNameEntry.path();
			const auto it = newInpxFolders.find(inpxFileName);
			if (it == newInpxFolders.end())
				continue;

			const Zip zip(QString::fromStdWString(inpxFileName));
			ParseInpxFiles(inpxFileName, &zip, it->second);
			result += it->second.size();
		}

		const auto filter = [] (auto & dst, const auto & src)
		{
			std::erase_if(dst, [&] (const auto & item){ return src.contains(item.first); });
		};

		filter(m_data.authors, oldData.authors);
		filter(m_data.series, oldData.series);
		filter(m_data.keywords, oldData.keywords);
		filter(m_data.folders, oldData.folders);

		if (const auto failsCount = Store(dbFileName, m_data); failsCount != 0)
			PLOGE << "Something went wrong";

		Analyze(dbFileName);

		return result;
	}

	void Parse()
	{
		Util::Timer t(L"parsing archives");

		auto [genresData, genresIndex] = LoadGenres(m_ini(GENRES, DEFAULT_GENRES));
		m_data.genres = std::move(genresData);
		m_genresIndex = std::move(genresIndex);
		SetUnknownGenreId();

		for (const auto & inpxFileNameEntry : GetInpxFilesInFolder(m_ini(INPX_FOLDER)))
		{
			const auto & inpxFileName = inpxFileNameEntry.path();
			const auto inpxContent = ExtractInpxFileNames(inpxFileName);

			const auto zip = [&]
			{
				if (!exists(inpxFileName))
					return std::unique_ptr<Zip>{};

				try
				{
					return std::make_unique<Zip>(QString::fromStdWString(inpxFileName.generic_wstring()));
				}
				catch (const std::exception & ex)
				{
					PLOGE << ex.what();
				}
				catch (...)
				{
					PLOGE << "Unknown error";
				}
				return std::unique_ptr<Zip>{};
			}();

			ParseInpxFiles(inpxFileName, zip.get(), inpxContent.inpx);
		}

		AddUnIndexedBooks();
		ScanUnIndexedFolders();
	}

	void AddUnIndexedBooks()
	{
		if (!(m_mode & CreateCollectionMode::AddUnIndexedFiles))
			return;

		for (const auto& [folder, files] : m_foldersContent | std::views::filter([](const auto& item) { return !item.second.empty(); }))
		{
			QFileInfo archiveFileInfo(QString::fromStdWString(m_ini(INPX_FOLDER) / folder));
			for (const Zip zip(archiveFileInfo.filePath()); const auto & fileName : files)
			{
				PLOGW << "Book is not indexed: " << ToMultiByte(folder) << "/" << fileName;
				ParseFile(folder, zip, QString::fromStdWString(fileName), archiveFileInfo.birthTime());
			}
		}
	}

	void ScanUnIndexedFolders()
	{
		if (!(m_mode & CreateCollectionMode::ScanUnIndexedFolders))
			return;

		for (auto const& entry : std::filesystem::recursive_directory_iterator(m_rootFolder))
		{
			if (entry.is_directory())
				continue;

			auto folder = entry.path().wstring();
			folder.erase(0, m_rootFolder.string().size() + 1);

			if (m_foldersContent.contains(folder))
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

	void ParseInpxFiles(const Path & inpxFileName, const Zip * zipInpx, const std::vector<std::wstring> & inpxFiles)
	{
		[[maybe_unused]] const auto * r = std::setlocale(LC_ALL, "en_US.utf8");  // NOLINT(concurrency-mt-unsafe)

		m_rootFolder = Path(inpxFileName).parent_path();
		if (zipInpx)
		{
			GetFieldList(*zipInpx);
			for (const auto & fileName : inpxFiles)
				GetDecodedStream(*zipInpx, fileName, [&] (QIODevice & zipDecodedStream)
			{
				ProcessInpx(zipDecodedStream, m_rootFolder, fileName);
			});

			PLOGI << m_n << " rows parsed";
		}

		LogErrors();
	}

	void GetFieldList(const Zip& zip)
	{
		const auto fieldList = [&]() -> QString
		{
			return zip.GetFileNameList().contains("structure.info")
				? QString::fromUtf8(zip.Read("structure.info")->GetStream().readAll())
				: QString("AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;LIBRATE;KEYWORDS;");
		}();

		const std::unordered_map<QString, BookBufFieldGetter> bookBufMapping{
#define		BOOK_BUF_FIELD_ITEM(NAME) { QString(#NAME), &Get##NAME },
			BOOK_BUF_FIELD_ITEMS_XMACRO
#undef		BOOK_BUF_FIELD_ITEM
		};

		auto fields = fieldList.split(';', Qt::SkipEmptyParts);
		if (const auto range = std::ranges::remove_if(fields, [](const auto& item) { return item.simplified().isEmpty(); }))
			fields.erase(range.begin(), range.end());
		m_bookBufMapping.clear();
		m_bookBufMapping.reserve(fields.size());
		std::ranges::transform(fields, std::back_inserter(m_bookBufMapping), [&](const auto& str)
			{
				const auto it = bookBufMapping.find(str.simplified());
				if (it == bookBufMapping.end())
				{
					PLOGF << "unexpected field name " << str;
					throw std::runtime_error("unexpected field name");
				}

				return it->second;
			});
	}

	void ProcessInpx(QIODevice & stream, const Path & rootFolder, std::wstring folder)
	{
		const auto mask = QString::fromStdWString(Path(folder).replace_extension("*"));
		QStringList suitableFiles = QDir(QString::fromStdWString(rootFolder)).entryList({ mask });
		std::ranges::transform(suitableFiles, suitableFiles.begin(), [] (const auto & file) { return file.toLower(); });

		folder = suitableFiles.isEmpty() ? Path(folder).replace_extension(ZIP).wstring() : suitableFiles.front().toStdWString();

		while (true)
		{
			const auto byteArray = stream.readLine();
			if (byteArray.isEmpty())
				break;

			auto line = ToWide(byteArray.constData());
			const auto buf = ParseBook(folder, line, m_bookBufMapping);

			auto& fileList = m_foldersContent[std::wstring(buf.FOLDER)];
			if (fileList.empty())
			{
				const QFileInfo archiveFileInfo(QString::fromStdWString(rootFolder / buf.FOLDER));
				if (archiveFileInfo.exists())
				{
					Zip archiveFile(archiveFileInfo.filePath());
					std::ranges::transform(archiveFile.GetFileNameList(), std::inserter(fileList, fileList.end()), [](const auto& item) { return item.toStdWString(); });
				}
			}

			const auto fileName = std::wstring(buf.FILE).append(L".").append(buf.EXT);
			const auto it = fileList.find(fileName);
			const auto found = it != fileList.end();
			if (found)
				fileList.erase(it);

			if (found || !(m_mode & CreateCollectionMode::SkipLostBooks))
				AddBook(buf);
			else
				PLOGW << std::quoted(ToMultiByte(buf.TITLE)) << " skipped because its file " << ToMultiByte(buf.FILE) << "." << ToMultiByte(buf.EXT) << " not found.";
		}
	}

	void ParseFile(const std::wstring & folder, const Zip & zip, const QString & fileName, const QDateTime & zipDateTime)
	{
		QFileInfo fileInfo(fileName);
		const auto stream = zip.Read(fileName);
		Fb2Parser parser(stream->GetStream(), fileName);
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
			<< parserData.keywords
			;

		auto line = values.join(FIELDS_SEPARATOR).toStdWString();
		const auto buf = ParseBook(folder, line, m_bookBufMapping);

		std::lock_guard lock(m_dataGuard);
		AddBook(buf);
	}

	void AddBook(const BookBuf & buf)
	{
		const auto id = GetId();
		auto file = ToMultiByte(buf.FILE) + "." + ToMultiByte(buf.EXT);

		std::ranges::transform(ParseItem(buf.AUTHOR, m_data.authors), std::back_inserter(m_data.booksAuthors), [=] (size_t idAuthor) { return std::make_pair(id, idAuthor); });

		auto idGenres = ParseItem(buf.GENRE, m_genresIndex, LIST_SEPARATOR, &ParseCheckerDefault,
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

		std::ranges::transform(idGenres, std::back_inserter(m_data.booksGenres), [&] (const size_t idGenre) { return std::make_pair(id, idGenre); });

		if (!buf.KEYWORDS.empty())
			std::ranges::transform(ParseKeywords(buf.KEYWORDS, m_data.keywords, m_uniqueKeywords), std::back_inserter(m_data.booksKeywords), [=](size_t idKeyword) { return std::make_pair(id, idKeyword); } );

		auto & idFolder = m_data.folders[std::wstring(buf.FOLDER)];
		if (idFolder == 0)
			idFolder = GetId();

		m_data.books.emplace_back(id
			, buf.LIBID
			, buf.TITLE
			, Add<int, -1>(buf.SERIES, m_data.series)
			, To<int>(buf.SERNO, -1)
			, buf.DATE, To<int>(buf.LIBRATE)
			, buf.LANG
			, idFolder
			, buf.FILE
			, To<size_t>(buf.INSNO)
			, buf.EXT
			, To<size_t>(buf.SIZE)
			, To<bool>(buf.DEL, false)
		);

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

	Path m_rootFolder;
	Data m_data;
	Dictionary m_genresIndex;
	size_t m_n { 0 };
	std::atomic_uint64_t m_parsedN { 0 };
	std::vector<std::wstring> m_unknownGenres;
	size_t m_unknownGenreId { 0 };
	std::unordered_map<QString, std::wstring> m_uniqueKeywords;
	std::unordered_map<std::wstring, std::unordered_set<std::wstring, CaseInsensitiveHash<std::wstring>>, CaseInsensitiveHash<std::wstring>> m_foldersContent;

	std::vector<QString> m_errors;

	BookBufMapping m_bookBufMapping;

	std::queue<std::wstring> m_foldersToParse;
	std::mutex m_foldersToParseGuard;
	std::condition_variable m_foldersToParseCondition;

	std::mutex m_dataGuard;

	std::vector<std::unique_ptr<Thread>> m_threads;
};

Parser::Parser() = default;
Parser::~Parser() = default;

void Parser::CreateNewCollection(IniMap data, const CreateCollectionMode mode, Callback callback)
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

void Parser::UpdateCollection(IniMap data, const CreateCollectionMode mode, Callback callback)
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
