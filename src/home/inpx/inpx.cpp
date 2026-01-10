#pragma warning(push, 0)

#include <QCryptographicHash>

#include <filesystem>
#include <fstream>
#include <future>
#include <queue>
#include <ranges>
#include <set>
#include <sstream>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>

#include "log.h"
#include "sqlite3ppext.h"

#pragma warning(pop)

#include "fnd/IsOneOf.h"
#include "fnd/try.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "util/Fb2InpxParser.h"
#include "util/IExecutor.h"
#include "util/executor/factory.h"
#include "util/language.h"
#include "util/timer.h"

#include "Constant.h"
#include "InpxConstant.h"
#include "inpx.h"
#include "types.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Inpx;
using namespace Util;

namespace
{

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

bool ParseCheckerAuthor(const std::wstring_view str)
{
	return !str.empty() && !std::ranges::all_of(str, [](const wchar_t ch) {
		return ch == L',';
	});
}

Dictionary::const_iterator FindDefault(const Dictionary& container, const std::wstring_view value)
{
	return container.find(value);
}

bool IsComment(const std::wstring_view line)
{
	return std::size(line) < 3 || line.starts_with(COMMENT_START);
}

class Ini
{
public:
	explicit Ini(Parser::IniMap data)
		: _data(std::move(data))
	{
	}

	const Path& operator()(const Parser::IniMap::value_type::first_type& key, const Path& defaultValue = {}) const
	{
		const auto it = _data.find(key);
		if (it != _data.end())
			return it->second;

		if (!defaultValue.empty())
			return defaultValue;

		assert(false && "value required");
		throw std::runtime_error("value required");
	}

	bool Exists(const Parser::IniMap::value_type::first_type& key) const
	{
		return _data.contains(key);
	}

private:
	Parser::IniMap _data;
};

class DatabaseWrapper
{
public:
	explicit DatabaseWrapper(const Path& dbFileName, const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
		: m_db(QString::fromStdWString(dbFileName).toUtf8(), flags)
		, m_func(m_db)
	{
		m_db.load_extension("MyHomeLibSQLIteExt");
		m_func.create("MHL_TRIGGERS_ON", [](sqlite3pp::ext::context& ctx) {
			ctx.result(1);
		});
	}

public:
	operator sqlite3pp::database&()
	{
		return m_db;
	}

	sqlite3pp::database* operator->()
	{
		return &m_db;
	}

private:
	sqlite3pp::database      m_db;
	sqlite3pp::ext::function m_func;
};

auto LoadGenres(const Path& genresIniFileName)
{
	Genres     genres;
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

		auto         it     = std::cbegin(line);
		const auto   codes  = Next(it, std::cend(line), GENRE_SEPARATOR);
		auto         itCode = std::cbegin(codes);
		std::wstring code;
		while (itCode != std::cend(codes))
		{
			const auto& added = index.emplace(Next(itCode, std::cend(codes), Fb2InpxParser::LIST_SEPARATOR), std::size(genres)).first->first;
			if (code.empty())
				code = added;
		}

		const auto parent = Next(it, std::cend(line), GENRE_SEPARATOR);
		const auto title  = Next(it, std::cend(line), GENRE_SEPARATOR);
		genres.emplace_back(code, parent, title);
	}

	std::for_each(std::next(std::begin(genres)), std::end(genres), [&index, &genres](Genre& genre) {
		const auto it = index.find(genre.parentCore);
		assert(it != index.end());
		auto& parent   = genres[it->second];
		genre.parentId = it->second;
		genre.dbCode   = ToWide(std::format("{:s}.{:03d}", ToMultiByte(parent.dbCode), ++parent.childrenCount));
	});

	return std::make_pair(std::move(genres), std::move(index));
}

template <typename T, T emptyValue = 0>
T Add(std::wstring_view value, Dictionary& container, const GetIdFunctor& getId = &GetIdDefault, const FindFunctor& find = &FindDefault)
{
	if (value.empty())
		return emptyValue;

	auto it = find(container, value);
	if (it == container.end())
		it = container.emplace(value, getId(value)).first;

	return static_cast<T>(it->second);
}

std::vector<size_t> ParseItem(
	const std::wstring_view data,
	Dictionary&             container,
	const wchar_t           separator    = Fb2InpxParser::LIST_SEPARATOR,
	const ParseChecker&     parseChecker = &ParseCheckerDefault,
	const GetIdFunctor&     getId        = &GetIdDefault,
	const FindFunctor&      find         = &FindDefault
)
{
	std::unordered_set<size_t> unique;
	std::vector<size_t>        result;
	auto                       it = std::ranges::find_if(data, [=](const auto ch) {
        return ch != separator;
    });
	while (it != std::cend(data))
		if (const auto value = Next(it, std::cend(data), separator); parseChecker(value))
			if (const auto [valueIt, added] = unique.insert(Add<size_t>(value, container, getId, find)); added)
				result.emplace_back(*valueIt);

	return result;
}

std::vector<size_t> ParseItem(const std::wstring_view data, Dictionary& container, const Splitter& splitter, const GetIdFunctor& getId = &GetIdDefault, const FindFunctor& find = &FindDefault)
{
	std::unordered_set<size_t> unique;
	std::vector<size_t>        result;
	for (const auto& value : splitter(data))
		if (const auto [valueIt, added] = unique.insert(Add<size_t>(value, container, getId, find)); added)
			result.emplace_back(*valueIt);

	return result;
}

std::vector<size_t> ParseKeywords(const std::wstring_view keywordsSrc, Dictionary& keywordsLinks, std::unordered_map<QString, std::wstring>& uniqueKeywords)
{
	return ParseItem(
		keywordsSrc,
		keywordsLinks,
		[&](const std::wstring_view item) {
			QStringList keywordsList;
			QString     keywordsStr = QString::fromWCharArray(item.data(), static_cast<int>(item.size()))
		                              .replace("--", ",")
		                              .replace(" - ", ",")
		                              .replace(" & ", " and ")
		                              .replace(QChar(L'\x0401'), QChar(L'\x0415'))
		                              .replace(QChar(L'\x0451'), QChar(L'\x0435'))
		                              .replace("_", " ");
			keywordsStr.remove(QRegularExpression(QString::fromStdWString(L"[@!\\?\"\x00ab\x00bb]")));
			auto list = keywordsStr.split(QRegularExpression(R"([,;#/\\\.\(\)\[\]])"), Qt::SkipEmptyParts);
			std::ranges::transform(list, list.begin(), [](const auto& str) {
				return str.simplified();
			});

			if (const auto [begin, end] = std::ranges::remove_if(
					list,
					[](const auto& str) {
						return str.length() < 3 || str.startsWith("DocId:", Qt::CaseInsensitive);
					}
				);
		        begin != end)
				list.erase(begin, end);
			assert(std::ranges::none_of(list, [](const auto& str) {
				return str.startsWith("DocId:", Qt::CaseInsensitive);
			}));

			for (auto& keyword : list)
				keywordsList << keyword.split(':', Qt::SkipEmptyParts);

			for (auto& keyword : keywordsList)
			{
				if (const auto it = std::ranges::find_if(
						keyword,
						[](const QChar& c) {
							return c.isLetterOrNumber() || IsOneOf(c, '+');
						}
					);
			        it != keyword.begin())
					keyword = keyword.last(std::distance(it, keyword.end()));
				keyword = keyword.simplified();
				if (!keyword.isEmpty())
					keyword[0] = keyword[0].toUpper();
			}
			if (const auto [begin, end] = std::ranges::remove_if(
					keywordsList,
					[](const auto& str) {
						return str.length() < 3;
					}
				);
		        begin != end)
				keywordsList.erase(begin, end);

			std::vector<std::wstring> result;
			result.reserve(keywordsList.size());
			std::ranges::transform(keywordsList, std::back_inserter(result), [](const auto& str) {
				return str.toStdWString();
			});
			return result;
		},
		&GetIdDefault,
		[&](const Dictionary& container, const std::wstring_view value) {
			auto key = QString::fromWCharArray(value.data(), static_cast<int>(value.size())).toLower();
			if (const auto it = uniqueKeywords.find(key); it != uniqueKeywords.end())
				return container.find(it->second);

			uniqueKeywords.try_emplace(std::move(key), value);
			return container.end();
		}
	);
}

using BookBufFieldGetter = std::wstring_view& (*)(BookBuf&);
using BookBufMapping     = std::vector<BookBufFieldGetter>;

#define BOOK_BUF_FIELD_ITEM(NAME)              \
	std::wstring_view& Get##NAME(BookBuf& buf) \
	{                                          \
		return buf.NAME;                       \
	}
BOOK_BUF_FIELD_ITEMS_XMACRO
#undef BOOK_BUF_FIELD_ITEM

BookBuf ParseBook(const std::wstring_view folder, std::wstring& line, const BookBufMapping& f)
{
	BookBuf buf { .FOLDER = folder };

	auto       it  = std::begin(line);
	const auto end = std::end(line);
	for (size_t i = 0, sz = f.size(); i < sz && it != end; ++i)
		f[i](buf) = Next(it, end, Fb2InpxParser::FIELDS_SEPARATOR);

	if (buf.GENRE.size() < 2)
		buf.GENRE = GENRE_NOT_SPECIFIED;

	return buf;
}

struct InpxContent
{
	std::vector<std::wstring> collectionInfo;
	std::vector<std::wstring> versionInfo;
	std::vector<std::wstring> inpx;
};

InpxContent ExtractInpxFileNames(const Path& inpxPath)
{
	if (!exists(inpxPath))
	{
		PLOGW << QString::fromStdWString(inpxPath) << " not found";
		return {};
	}

	InpxContent inpxContent;

	const auto inpxFileName = QString::fromStdWString(inpxPath);
	const auto zip          = TRY(QString("open %1").arg(inpxFileName), [&] {
        return std::make_unique<Zip>(inpxFileName);
    });
	if (!zip)
		return inpxContent;

	for (const auto& fileName : zip->GetFileNameList())
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

	PLOGD << QString("%1 content: connection info: %2, version info: %3, inp: %4").arg(inpxFileName).arg(inpxContent.collectionInfo.size()).arg(inpxContent.versionInfo.size()).arg(inpxContent.inpx.size());

	return inpxContent;
}

void GetDecodedStream(const Zip& zip, const std::wstring& file, const std::function<void(QIODevice&)>& f)
{
	PLOGI << file;
	TRY("get decoded stream", [&] {
		const auto stream = zip.Read(QString::fromStdWString(file));
		f(stream->GetStream());
		return 0;
	});
}

bool ExecuteScript(const std::wstring& action, const Path& dbFileName, const Path& scriptFileName)
{
	Timer t(action);

	DatabaseWrapper db(dbFileName);

	std::ifstream inp(scriptFileName);
	if (!inp.is_open())
		throw std::runtime_error(std::format("Cannot open {}", scriptFileName.generic_string()));

	while (!inp.eof())
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

	return true;
}

void SetIsNavigationDeleted(DatabaseWrapper& db)
{
	for (const auto& [table, where, _, join, additional] : IS_DELETED_UPDATE_ARGS)
	{
		PLOGI << "set IsDeleted for " << table;
		const auto query = QString(IS_DELETED_UPDATE_STATEMENT_TOTAL).arg(table, join, where, additional).toStdString();
#ifndef NDEBUG
		PLOGD << query;
#endif
		[[maybe_unused]] const auto rc = sqlite3pp::command(db, query.data()).execute();
		assert(rc == 0);
	}
}

int Analyze(const Path& dbFileName)
{
	DatabaseWrapper db(dbFileName);
	SetIsNavigationDeleted(db);
	PLOGI << "ANALYZE";
	auto rc = sqlite3pp::command(db, "ANALYZE").execute();
	assert(rc == 0);

	sqlite3pp::query query(db, "select exists (select 42 from Books where Year is not null)");
	rc = sqlite3pp::command(db, std::format("insert or replace into Settings(SettingID, SettingValue) values(1, '{}')", (*query.begin()).get<int>(0) ? "" : "Year").data()).execute();
	assert(rc == 0);

	return rc;
}

void WriteDatabaseVersion(const Path& dbFileName, const std::wstring& statement)
{
	DatabaseWrapper             db(dbFileName);
	[[maybe_unused]] const auto rc = sqlite3pp::command(db, ToMultiByte(statement).data()).execute();
	assert(rc == 0);
}

template <typename Container, typename Functor>
size_t StoreRange(const Path& dbFileName, std::string_view process, const std::string_view query, const Container& container, Functor&& f, const std::string_view queryAfter = {})
{
	auto impl = [&] {
		const auto rowsTotal = std::size(container);
		if (rowsTotal == 0)
			return rowsTotal;

		Timer  t(ToWide(std::format("store {0} {1}", process, rowsTotal)));
		size_t rowsInserted = 0;

		DatabaseWrapper        db(dbFileName);
		sqlite3pp::transaction tr(db);
		sqlite3pp::command     cmd(db, query.data());

		const auto log = [rowsTotal, &rowsInserted] {
			PLOGD << std::format("{0} rows inserted ({1}%)", rowsInserted, rowsInserted * 100 / rowsTotal);
		};

		const auto result =
			std::accumulate(std::cbegin(container), std::cend(container), static_cast<size_t>(0), [f = std::forward<Functor>(f), &db, &cmd, &rowsInserted, &log](const size_t init, const auto& value) {
				const auto localResult = f(cmd, value) + cmd.reset();

				if (localResult == 0)
				{
					if (++rowsInserted % LOG_INTERVAL == 0)
						log();
				}
				else
				{
					assert(false);
					PLOGE << db->error_code() << ": " << db->error_msg();
				}

				return init + static_cast<size_t>(localResult);
			});

		log();
		if (rowsTotal != rowsInserted)
			PLOGE << rowsTotal - rowsInserted << " rows lost";

		if (!queryAfter.empty())
			sqlite3pp::command(db, queryAfter.data()).execute();

		{
			Timer tc(L"commit");
			tr.commit();
		}

		return result;
	};

	return TRY(process, impl);
}

size_t Store(const Path& dbFileName, Data& data)
{
	size_t result  = 0;
	result        += StoreRange(
        dbFileName,
        "Authors",
        "INSERT INTO Authors (AuthorID, LastName, FirstName, MiddleName, SearchName) VALUES(?, ?, ?, ?, MHL_UPPER(?))",
        data.authors,
        [](sqlite3pp::command& cmd, const Dictionary::value_type& item) {
            const auto& [author, id] = item;
            auto       it            = std::cbegin(author);
            const auto last          = ToMultiByte(Next(it, std::cend(author), Fb2InpxParser::NAMES_SEPARATOR));
            const auto first         = ToMultiByte(Next(it, std::cend(author), Fb2InpxParser::NAMES_SEPARATOR));
            const auto middle        = ToMultiByte(Next(it, std::cend(author), Fb2InpxParser::NAMES_SEPARATOR));

            cmd.bind(1, id);
            cmd.bind(2, last, sqlite3pp::nocopy);
            cmd.bind(3, first, sqlite3pp::nocopy);
            cmd.bind(4, middle, sqlite3pp::nocopy);
            cmd.bind(5, last, sqlite3pp::nocopy);

            return cmd.execute();
        },
        "INSERT INTO Authors_Search(Authors_Search) VALUES('rebuild')"
    );

	result += StoreRange(
		dbFileName,
		"Series",
		"INSERT INTO Series (SeriesID, SeriesTitle, SearchTitle) VALUES (?, ?, MHL_UPPER(?))",
		data.series,
		[](sqlite3pp::command& cmd, const Dictionary::value_type& item) {
			const auto title = ToMultiByte(item.first);
			cmd.bind(1, item.second);
			cmd.bind(2, title, sqlite3pp::nocopy);
			cmd.bind(3, title, sqlite3pp::nocopy);
			return cmd.execute();
		},
		"INSERT INTO Series_Search(Series_Search) VALUES('rebuild')"
	);

	result += StoreRange(dbFileName, "Folders", "INSERT INTO Folders (FolderID, FolderTitle) VALUES (?, ?)", data.bookFolders, [](sqlite3pp::command& cmd, const auto& item) {
		const auto title = ToMultiByte(item.first);
		cmd.bind(1, item.second);
		cmd.bind(2, title, sqlite3pp::nocopy);
		return cmd.execute();
	});

	result += StoreRange(dbFileName, "InpxFolders", "INSERT INTO Inpx (Folder, File, Hash) VALUES (?, ?, ?)", data.inpxFolders, [](sqlite3pp::command& cmd, const auto& item) {
		const auto folder = ToMultiByte(item.first.first);
		const auto file   = ToMultiByte(item.first.second);
		cmd.bind(1, folder, sqlite3pp::nocopy);
		cmd.bind(2, file, sqlite3pp::nocopy);
		cmd.bind(3, item.second, sqlite3pp::nocopy);
		return cmd.execute();
	});

	result += StoreRange(
		dbFileName,
		"Keywords",
		"INSERT INTO Keywords (KeywordID, KeywordTitle, SearchTitle) VALUES (?, ?, MHL_UPPER(?))",
		data.keywords,
		[](sqlite3pp::command& cmd, const Dictionary::value_type& item) {
			const auto title = ToMultiByte(item.first);
			cmd.bind(1, item.second);
			cmd.bind(2, title, sqlite3pp::nocopy);
			cmd.bind(3, title, sqlite3pp::nocopy);
			return cmd.execute();
		}
	);

	std::vector<size_t> newGenresIndex;
	for (size_t i = 0, sz = std::size(data.genres); i < sz; ++i)
		if (data.genres[i].newGenre)
			newGenresIndex.push_back(i);
	result += StoreRange(
		dbFileName,
		"Genres",
		"INSERT INTO Genres (GenreCode, ParentCode, FB2Code, GenreAlias) VALUES(?, ?, ?, ?)",
		newGenresIndex | std::views::drop(1),
		[&genres = data.genres](sqlite3pp::command& cmd, const size_t n) {
			cmd.binder() << ToMultiByte(genres[n].dbCode) << ToMultiByte(genres[genres[n].parentId].dbCode) << ToMultiByte(genres[n].code) << ToMultiByte(genres[n].name);
			return cmd.execute();
		}
	);

	const char* queryText  = "INSERT INTO Books ("
							 "BookID   , LibID     , Title    , SeriesID, "
							 "SeqNumber, UpdateDate, LibRate  , Lang    , Year, "
							 "FolderID , FileName  , InsideNo , Ext     , "
							 "BookSize , IsDeleted, UpdateId, SourceLib, SearchTitle"
							 ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, MHL_UPPER(?))";
	result                += StoreRange(
        dbFileName,
        "Books",
        queryText,
        data.books,
        [](sqlite3pp::command& cmd, const Book& book) {
            const auto libId    = ToMultiByte(book.libId);
            const auto title    = ToMultiByte(book.title);
            const auto date     = ToMultiByte(book.date);
            const auto language = ToMultiByte(book.language);
            const auto fileName = ToMultiByte(book.fileName);
            const auto format   = ToMultiByte(book.format);

            cmd.bind(1, book.id);
            cmd.bind(2, libId, sqlite3pp::nocopy);
            cmd.bind(3, title, sqlite3pp::nocopy);
            book.seriesId == -1 ? cmd.bind(4, sqlite3pp::null_type()) : cmd.bind(4, book.seriesId);
            book.seriesNum == -1 ? cmd.bind(5, sqlite3pp::null_type()) : cmd.bind(5, book.seriesNum);
            cmd.bind(6, date, sqlite3pp::nocopy);
            cmd.bind(7, book.rate);
            cmd.bind(8, language, sqlite3pp::nocopy);
            book.year == -1 ? cmd.bind(9, sqlite3pp::null_type()) : cmd.bind(9, book.year);
            cmd.bind(10, book.folder);
            cmd.bind(11, fileName, sqlite3pp::nocopy);
            cmd.bind(12, book.insideNo);
            cmd.bind(13, format, sqlite3pp::nocopy);
            cmd.bind(14, book.size);
            cmd.bind(15, book.deleted ? 1 : 0);
            cmd.bind(16, book.updateId);
            cmd.bind(17, book.sourceLib, sqlite3pp::nocopy);
            cmd.bind(18, title, sqlite3pp::nocopy);
            return cmd.execute();
        },
        "INSERT INTO Books_Search(Books_Search) VALUES('rebuild')"
    );

	{
		std::unordered_set<std::wstring> languages;
		std::ranges::transform(data.books, std::inserter(languages, languages.end()), [](const auto& item) {
			return item.language;
		});
		result += StoreRange(dbFileName, "Languages", "INSERT OR IGNORE INTO Languages (LanguageCode) VALUES(?)", languages, [](sqlite3pp::command& cmd, const std::wstring& item) {
			const auto language = ToMultiByte(item);
			cmd.reset();
			cmd.binder() << language;
			return cmd.execute();
		});
	}

	result += StoreRange(dbFileName, "Author_List", "INSERT INTO Author_List (AuthorID, BookID, OrdNum) VALUES(?, ?, ?)", data.booksAuthors, [](sqlite3pp::command& cmd, const Links::value_type& item) {
		return std::accumulate(item.second.cbegin(), item.second.cend(), 0, [&, ordNum = 0](const int init, const size_t id) mutable {
			cmd.reset();
			cmd.binder() << id << item.first << ++ordNum;
			return init + cmd.execute();
		});
	});

	result += StoreRange(
		dbFileName,
		"Series_List",
		"INSERT INTO Series_List (SeriesID, BookID, SeqNumber, OrdNum) VALUES(?, ?, ?, ?)",
		data.booksSeries,
		[](sqlite3pp::command& cmd, const BooksSeries::value_type& item) {
			return std::accumulate(item.second.cbegin(), item.second.cend(), 0, [&, ordNum = 0](const int init, const std::pair<size_t, std::optional<int>> series) mutable {
				cmd.reset();
				cmd.bind(1, series.first);
				cmd.bind(2, item.first);
				if (series.second)
					cmd.bind(3, *series.second);
				else
					cmd.bind(3);
				cmd.bind(4, ++ordNum);
				return init + cmd.execute();
			});
		}
	);

	result += StoreRange(dbFileName, "Keyword_List", "INSERT INTO Keyword_List (KeywordID, BookID, OrdNum) VALUES(?, ?, ?)", data.booksKeywords, [](sqlite3pp::command& cmd, const Links::value_type& item) {
		return std::accumulate(item.second.cbegin(), item.second.cend(), 0, [&, ordNum = 0](const int init, const size_t id) mutable {
			cmd.reset();
			cmd.binder() << id << item.first << ++ordNum;
			return init + cmd.execute();
		});
	});
	std::vector<std::string> genres;
	genres.reserve(std::size(genres));
	std::ranges::transform(data.genres, std::back_inserter(genres), [](const auto& item) {
		return ToMultiByte(item.dbCode);
	});
	result += StoreRange(
		dbFileName,
		"Genre_List",
		"INSERT INTO Genre_List (BookID, GenreCode, OrdNum) VALUES(?, ?, ?)",
		data.booksGenres,
		[genres = std::move(genres)](sqlite3pp::command& cmd, const Links::value_type& item) {
			return std::accumulate(item.second.cbegin(), item.second.cend(), 0, [&, ordNum = 0](const int init, const size_t id) mutable {
				assert(id < std::size(genres));
				cmd.reset();
				cmd.bind(1, item.first);
				cmd.bind(2, genres[id], sqlite3pp::nocopy);
				cmd.bind(3, ++ordNum);
				return init + cmd.execute();
			});
		}
	);

	std::vector<std::tuple<size_t, int, size_t>> updates;

	std::vector<const Update*> stack;
	std::ranges::transform(data.updates.children | std::views::values, std::back_inserter(stack), [](const auto& item) {
		return &item;
	});
	while (!stack.empty())
	{
		const auto* update = stack.back();
		stack.pop_back();
		updates.emplace_back(update->id, update->title, update->parentId);
		std::ranges::transform(update->children | std::views::values, std::back_inserter(stack), [](const auto& item) {
			return &item;
		});
	}

	result += StoreRange(dbFileName, "Updates", "INSERT INTO Updates (UpdateID, UpdateTitle, ParentID) VALUES(?, ?, ?)", updates, [](sqlite3pp::command& cmd, const auto& item) {
		cmd.binder() << static_cast<long long>(std::get<0>(item)) << std::get<1>(item) << static_cast<long long>(std::get<2>(item));
		return cmd.execute();
	});

	if (!data.reviews.empty())
	{
		std::vector<std::pair<size_t, std::wstring>> reviews;

		{
			DatabaseWrapper db(dbFileName);

			sqlite3pp::query query(db, "select b.BookID, f.FolderTitle||'#'||b.FileName||b.Ext from Books b join Folders f on f.FolderID = b.FolderID");
			std::for_each(query.begin(), query.end(), [&](const auto& row) {
				int64_t     bookId;
				const char* uid;
				std::tie(bookId, uid) = row.template get_columns<int64_t, const char*>(0, 1);

				if (const auto it = data.reviews.find(uid); it != data.reviews.end())
				{
					std::ranges::transform(it->second, std::back_inserter(reviews), [&](const auto& item) {
						return std::make_pair(bookId, item);
					});
				}
			});
		}

		result += StoreRange(dbFileName, "Reviews", "INSERT INTO Reviews (BookID, Folder) VALUES(?, ?)", reviews, [](sqlite3pp::command& cmd, const auto& item) {
			cmd.binder() << static_cast<long long>(item.first) << ToMultiByte(item.second);
			return cmd.execute();
		});
	}

	return result;
}

std::vector<Path> GetInpxFilesInFolder(const Ini& ini)
{
	PLOGV << "GetInpxFilesInFolder started";
	if (ini.Exists(INPX_PATH))
		if (auto explicitInpxPath = ini(INPX_PATH); !explicitInpxPath.empty())
			return { std::move(explicitInpxPath) };

	auto result = std::filesystem::directory_iterator { ini(ARCHIVE_FOLDER) } | std::views::filter([](const Path& path) {
					  const auto ext  = path.extension().wstring();
					  const auto proj = [](const auto& ch) {
						  return std::tolower(ch);
					  };
					  return std::ranges::equal(ext, std::wstring(INPX_EXT), {}, proj, proj);
				  })
	            | std::views::transform([](const auto& entry) {
					  return entry.path();
				  })
	            | std::ranges::to<std::vector<Path>>();

	PLOGV << "GetInpxFilesInFolder finished";
	return result;
}

InpxFolders GetInpxFolder(const Ini& ini, const bool needHashes)
{
	InpxFolders folders;
	for (const auto& inpxFileNamePath : GetInpxFilesInFolder(ini))
	{
		const auto inpxFileName = QString::fromStdWString(inpxFileNamePath);
		PLOGV << "check " << inpxFileName << " started";

		const auto zip = TRY(QString("open %1").arg(inpxFileName), [&] {
			return std::make_unique<Zip>(inpxFileName);
		});
		if (!zip)
			continue;

		for (const auto& fileName : zip->GetFileNameList())
		{
			if (QFileInfo(fileName).suffix() != "inp")
				continue;

			auto hash = needHashes ? [&] {
				const auto         bytes = zip->Read(fileName)->GetStream().readAll();
				QCryptographicHash engine(QCryptographicHash::Md5);
				engine.addData(bytes);
				return QString::fromUtf8(engine.result().toHex()).toUpper().toStdString();
			}()
			                       : std::string {};

			folders.try_emplace(std::make_pair(inpxFileNamePath.filename().wstring(), fileName.toStdWString()), std::move(hash));
		}

		PLOGV << "check " << inpxFileName << " finished";
	}
	return folders;
}

template <typename T>
T ReadDictionary(const std::string_view name, sqlite3pp::database& db, const char* statement, const auto& f)
{
	PLOGI << "Read " << name;
	T                data;
	sqlite3pp::query query(db, statement);
	std::transform(std::begin(query), std::end(query), std::inserter(data, std::end(data)), f);
	return data;
}

Update ReadUpdates(sqlite3pp::database& db)
{
	PLOGI << "Read updates";
	Update                              result;
	std::unordered_map<size_t, Update*> updates {
		{ 0, &result },
	};
	sqlite3pp::query query(db, "select UpdateID, UpdateTitle, ParentID from Updates order by ParentId");
	for (auto it : query)
	{
		const auto id       = it.get<long long>(0);
		const auto title    = it.get<int>(1);
		const auto parentId = it.get<long long>(2);

		const auto parentIt = updates.find(parentId);
		assert(parentIt != updates.end());
		auto& child = parentIt->second->children.try_emplace(title, Update { static_cast<size_t>(id), title, static_cast<size_t>(parentId) }).first->second;
		updates.try_emplace(id, &child);
	}

	return result;
}

Reviews ReadReviews(sqlite3pp::database& db)
{
	PLOGI << "Read reviews";
	Reviews          reviews;
	sqlite3pp::query query(db, "select f.FolderTitle||'#'||b.FileName||b.Ext, r.Folder from Reviews r join Books b on b.BookID = r.BookID join Folders f on f.FolderID = b.FolderID");
	std::for_each(std::begin(query), std::end(query), [&](const auto& row) {
		const char* uid;
		const char* folder;
		std::tie(uid, folder) = row.template get_columns<const char*, const char*>(0, 1);
		reviews[uid].emplace(ToWide(folder));
	});

	return reviews;
}

std::pair<Genres, Dictionary> ReadGenres(sqlite3pp::database& db, const Path& genresFileName)
{
	PLOGI << "Read genres";
	std::pair<Genres, Dictionary> result;
	auto& [genres, index] = result;

	genres.emplace_back(L"0");
	index.emplace(genres.front().code, size_t { 0 });

	sqlite3pp::query                            query(db, "select FB2Code, GenreCode, ParentCode, GenreAlias from Genres");
	std::map<std::wstring, std::vector<size_t>> children;
	std::transform(std::begin(query), std::end(query), std::inserter(genres, std::end(genres)), [&, n = std::size(genres)](const auto& row) mutable {
		const auto fb2Code    = row.template get<std::string>(0);
		const auto genreCode  = row.template get<std::string>(1);
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
			genres[n].parentId   = i;
			genres[n].parentCore = genres[i].code;
		}
	}

	auto n = std::size(genres);

	auto [iniGenres, iniIndex] = LoadGenres(genresFileName);
	for (const auto& genre : iniGenres)
	{
		if (index.contains(genre.code))
			continue;

		index.emplace(genre.code, std::size(genres));
		genres.push_back(genre);
	}

	for (const auto sz = std::size(genres); n < sz; ++n)
	{
		const auto& parent = iniGenres[iniGenres[n].parentId];
		const auto  it     = index.find(parent.code);
		assert(it != index.end());
		iniGenres[n].parentId = it->second;
	}

	for (const auto& [code, k] : iniIndex)
	{
		if (index.contains(code))
			continue;

		const auto& genre = iniGenres[k];
		const auto  it    = index.find(genre.code);
		assert(it != index.end());
		index.emplace(code, it->second);
	}

	return result;
}

void SetNextId(sqlite3pp::database& db)
{
	sqlite3pp::query query(db, GET_MAX_ID_QUERY);
	g_id = (*query.begin()).get<long long>(0);
	PLOGD << "Next Id: " << g_id;
}

class IPool // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IPool()                                    = default;
	virtual void Work(std::promise<void>& startPromise) = 0;
};

class Thread
{
	NON_COPY_MOVABLE(Thread)

public:
	explicit Thread(IPool& pool)
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
	std::thread        m_thread;
};

} // namespace

class Parser::Impl final : virtual public IPool
{
	NON_COPY_MOVABLE(Impl)

public:
	using ArchiveParser = size_t (Impl::*)();

	static ArchiveParser GetNewInpxInpxFilesParser() noexcept
	{
		return &Impl::ParseNewInpxFiles;
	}

	static ArchiveParser GetNewArchivesParser() noexcept
	{
		return &Impl::ParseNewArchives;
	}

public:
	Impl(Ini ini, const CreateCollectionMode mode, Callback callback)
		: m_ini { std::move(ini) }
		, m_mode { mode }
		, m_callback { std::move(callback) }
		, m_executor { ExecutorFactory::Create(ExecutorImpl::Async) }
		, m_languageMapping { QString::fromStdWString(m_ini(LANGUAGES_MAPPING)) }
	{
	}

	~Impl() override
	{
		LogErrors();
	}

	void Process()
	{
		auto process = [&] {
			bool processed = false;
			ProcessImpl(processed);
			return processed;
		};
		(*m_executor)({ "Create collection", [&, process] {
						   const auto ok = TRY("create collection", process);

						   const auto genres = static_cast<size_t>(std::ranges::count_if(
												   m_data.genres,
												   [](const Genre& genre) {
													   return genre.newGenre;
												   }
											   ))
			                                 - 1;
						   return [this, genres, hasError = !ok](size_t) {
							   m_callback(UpdateResult { m_data.bookFolders.size(), m_data.authors.size(), m_data.series.size(), m_data.books.size(), m_data.keywords.size(), genres, false, hasError });
						   };
					   } });
	}

	void UpdateDatabase(const ArchiveParser archiveParser)
	{
		auto parse = [this, archiveParser]() {
			return UpdateDatabaseImpl(archiveParser);
		};
		(*m_executor)({ "Update collection", [&, parse] {
						   auto       foldersCount = TRY("update collection", parse);
						   const auto genres       = static_cast<size_t>(std::ranges::count_if(
                                                   m_data.genres,
                                                   [](const Genre& genre) {
                                                       return genre.newGenre;
                                                   }
                                               ))
			                                 - 1;
						   return [this, foldersCount, genres](size_t) {
							   m_callback(UpdateResult { foldersCount, m_data.authors.size(), m_data.series.size(), m_data.books.size(), m_data.keywords.size(), genres, m_oldDataUpdateFound });
						   };
					   } });
	}

private: // IPool
	void Work(std::promise<void>& startPromise) override
	{
		startPromise.set_value();

		while (true)
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

			auto work = [&] {
				const QFileInfo archiveFileInfo(QString::fromStdWString(m_ini(ARCHIVE_FOLDER) / folder));
				const auto      archiveFileName = archiveFileInfo.filePath();
				const auto      zip             = TRY(QString("open %1").arg(archiveFileName), [&] {
                    return std::make_unique<Zip>(archiveFileName);
                });
				if (!zip)
					return -1;

				const auto zipFileList = zip->GetFileNameList();
				for (size_t counter = 0; const auto& fileName : zipFileList)
				{
					++counter;
					if (QFileInfo(fileName).suffix() != "fb2")
						continue;

					PLOGD << "parsing " << folder << "/" << fileName << "  " << counter << " (" << zipFileList.size() << ") " << 100 * counter / zipFileList.size() << "%";
					auto process = [&] {
						return ParseFile(folder, *zip, fileName, archiveFileInfo.birthTime());
					};
					TRY(QString("parse %1").arg(fileName), process);
				}

				return 0;
			};

			TRY(QString("parsing %1").arg(QString::fromStdWString(folder)), work);
		}
	}

private:
	void ProcessImpl(bool& ok)
	{
		ok = false;
		Timer t(L"work");

		const auto& dbFileName = m_ini(DB_PATH);

		ExecuteScript(L"create database", dbFileName, m_ini(DB_CREATE_SCRIPT, DEFAULT_DB_CREATE_SCRIPT));
		WriteDatabaseVersion(dbFileName, m_ini(SET_DATABASE_VERSION_STATEMENT));

		Parse();
		if (const auto failsCount =
		        TRY("store",
		            [&] {
						return Store(dbFileName, m_data);
					});
		    failsCount != 0)
			PLOGE << "Something went wrong";

		TRY("update database", [&] {
			return ExecuteScript(L"update database", dbFileName, m_ini(DB_UPDATE_SCRIPT, DEFAULT_DB_UPDATE_SCRIPT));
		});

		TRY("CollectCompilations", [this] {
			CollectCompilations();
			return 0;
		});

		TRY("analyze", [&] {
			return Analyze(dbFileName);
		});

		ok = true;
	}

	void SetUnknownGenreId()
	{
		static constexpr auto UNKNOWN = L"unknown_root";
		const auto            it      = m_genresIndex.find(UNKNOWN);
		assert(it != m_genresIndex.end());
		m_unknownGenreId = it->second;
	}

	auto GetNewInpxFolders()
	{
		InpxFolders diff;
		const auto  inpxFolders = GetInpxFolder(m_ini, true);

		std::unordered_map<std::wstring, std::vector<std::wstring>> result;

		const auto proj = [](const auto& item) {
			return item.first;
		};
		std::ranges::set_difference(inpxFolders, m_data.inpxFolders, std::inserter(diff, diff.end()), {}, proj, proj);

		auto                  inpxCaches = inpxFolders | std::views::values;
		std::set<std::string> inpxCachesSorted { std::cbegin(inpxCaches), std::cend(inpxCaches) };

		auto                  dbCaches = m_data.inpxFolders | std::views::values;
		std::set<std::string> dbCachesSorted { std::cbegin(dbCaches), std::cend(dbCaches) };

		if (!std::ranges::includes(inpxCachesSorted, dbCachesSorted))
			m_oldDataUpdateFound = true;

		for (const auto& [folder, file] : diff | std::views::keys)
			result[folder].emplace_back(file);

		return result;
	}

	std::pair<Data, Dictionary> ReadData(const Path& dbFileName, const Path& genresFileName)
	{
		Data data;

		const auto dictionaryInserter = [](const auto& row) {
			int64_t     id;
			const char* value;
			std::tie(id, value) = row.template get_columns<int64_t, const char*>(0, 1);
			return std::make_pair(ToWide(value), static_cast<size_t>(id));
		};

		DatabaseWrapper db(dbFileName, SQLITE_OPEN_READONLY);
		SetNextId(db);
		data.authors               = ReadDictionary<Dictionary>("authors", db, "select AuthorID, LastName||','||FirstName||','||MiddleName from Authors", dictionaryInserter);
		data.series                = ReadDictionary<Dictionary>("series", db, "select SeriesID, SeriesTitle from Series", dictionaryInserter);
		data.keywords              = ReadDictionary<Dictionary>("keywords", db, "select KeywordID, KeywordTitle from Keywords", dictionaryInserter);
		data.bookFolders           = ReadDictionary<Folders>("folders", db, "select FolderID, FolderTitle from Folders", dictionaryInserter);
		auto [genres, genresIndex] = ReadGenres(db, genresFileName);
		data.genres                = std::move(genres);
		data.updates               = ReadUpdates(db);
		data.reviews               = ReadReviews(db);

		{
			sqlite3pp::query query(db, "select BookID, FolderID, FileName||Ext from Books");
			std::transform(std::begin(query), std::end(query), std::inserter(m_uniqueFiles, std::end(m_uniqueFiles)), [](const auto& row) {
				int64_t     bookId, folderId;
				const char* value;
				std::tie(bookId, folderId, value) = row.template get_columns<int64_t, int64_t, const char*>(0, 1, 2);
				return std::make_pair(std::make_pair(static_cast<size_t>(folderId), std::string(value)), bookId);
			});
		}

		const auto inpxFolderInserter = [](const auto& row) {
			std::string folder, file, hash;
			std::tie(folder, file, hash) = row.template get_columns<const char*, const char*, const char*>(0, 1, 2);
			return std::make_pair(std::make_pair(ToWide(folder), ToWide(file)), std::move(hash));
		};
		data.inpxFolders = ReadDictionary<InpxFolders>("inpx", db, "select Folder, File, Hash from Inpx", inpxFolderInserter);

		return std::make_pair(std::move(data), std::move(genresIndex));
	}

	void Filter(Update& dst) const
	{
		Update buf;
		std::swap(dst, buf);
		Update*    last   = &dst;
		const auto filter = [&](const Update& update, const auto& f) -> void {
			if (m_newUpdates.contains(update.id))
			{
				const auto [it, ok] = last->children.try_emplace(update.title, Update { update.id, update.title, update.parentId, {} });
				assert(ok);
				last = &it->second;
			}
			for (const auto& child : update.children | std::views::values)
				f(child, f);
		};
		filter(buf, filter);
	}

	static void Filter(Reviews& dst, const Reviews& src)
	{
		Reviews result;

		const auto proj = [](const auto& item) {
			return item.first;
		};
		std::ranges::set_difference(dst, src, std::inserter(result, result.end()), {}, proj, proj);
		for (auto itDst = dst.cbegin(), itSrc = src.cbegin(); itDst != dst.cend() && itSrc != src.cend();)
		{
			if (itDst->first < itSrc->first)
			{
				++itDst;
				continue;
			}

			if (itSrc->first < itDst->first)
			{
				++itSrc;
				continue;
			}

			assert(itDst->first == itSrc->first);
			std::vector<std::wstring> folders;
			std::ranges::set_difference(itDst->second, itSrc->second, std::back_inserter(folders));
			if (!folders.empty())
			{
				auto& resultFolders = result[itDst->first];
				std::ranges::move(std::move(folders), std::inserter(resultFolders, resultFolders.end()));
			}
			++itDst;
			++itSrc;
		}

		dst = std::move(result);
	}

	size_t ParseNewInpxFiles()
	{
		size_t     result         = 0;
		const auto newInpxFolders = GetNewInpxFolders();
		for (const auto& inpxPath : GetInpxFilesInFolder(m_ini))
		{
			const auto it = newInpxFolders.find(inpxPath.filename());
			if (it == newInpxFolders.end())
				continue;

			const auto inpxFileName = QString::fromStdWString(inpxPath);
			const auto zip          = TRY(QString("open %1").arg(inpxFileName), [&] {
                return std::make_unique<Zip>(inpxFileName);
            });
			if (!zip)
				continue;

			ParseInpxFiles(inpxPath, zip.get(), it->second);
			result += it->second.size();
		}

		return result;
	}

	size_t ParseNewArchives()
	{
		const auto inpxFolder = m_ini(ARCHIVE_FOLDER);
		auto       folders    = TRY(std::format("iterate {}", inpxFolder.generic_string()), [&] {
            std::vector<std::wstring> result;
            std::ranges::move(
                std::filesystem::directory_iterator(inpxFolder) | std::views::filter([](const auto& item) {
                    return !item.is_directory();
                }) | std::views::transform([&](const auto& item) {
                    auto folder = item.path().wstring();
                    folder.erase(0, inpxFolder.string().size() + 1);
                    return folder;
                }) | std::views::filter([&](const auto& item) {
                    return !(IsOneOf(item, COMPILATIONS, CONTENTS) || m_data.bookFolders.contains(item)) && Zip::IsArchive(QString::fromStdWString(inpxFolder / item));
                }),
                std::back_inserter(result)
            );
            return result;
        });

		const auto result = folders.size();

		std::ranges::for_each(std::move(folders), [this](auto&& item) {
			m_foldersToParse.push(std::forward<std::wstring>(item));
		});

		GetFieldList();

		TRY("scan archives", [this] {
			ScanUnIndexedFoldersImpl();
			return true;
		});

		return result;
	}

	size_t UpdateDatabaseImpl(ArchiveParser archiveParser)
	{
		const auto& dbFileName               = m_ini(DB_PATH);
		const auto [oldData, oldGenresIndex] = ReadData(dbFileName, m_ini(GENRES, DEFAULT_GENRES));

		std::ranges::transform(oldData.keywords, std::inserter(m_uniqueKeywords, m_uniqueKeywords.end()), [](const auto& item) {
			return std::pair(QString::fromStdWString(item.first).toLower(), item.first);
		});

		m_data        = oldData;
		m_genresIndex = oldGenresIndex;
		SetUnknownGenreId();

		size_t result = std::invoke(archiveParser, this);

		m_data.reviews.clear();
		TRY("CollectReviews", [this] {
			CollectReviews();
			return 0;
		});

		const auto filter = [](auto& dst, const auto& src) {
			std::erase_if(dst, [&](const auto& item) {
				return src.contains(item.first);
			});
		};

		filter(m_data.authors, oldData.authors);
		filter(m_data.series, oldData.series);
		filter(m_data.keywords, oldData.keywords);
		filter(m_data.bookFolders, oldData.bookFolders);
		filter(m_data.inpxFolders, oldData.inpxFolders);
		Filter(m_data.updates);
		Filter(m_data.reviews, oldData.reviews);

		if (const auto failsCount = Store(dbFileName, m_data); failsCount != 0)
			PLOGE << "Something went wrong";

		TRY("CollectCompilations", [this] {
			CollectCompilations();
			return 0;
		});

		TRY("analyze", [&] {
			return Analyze(dbFileName);
		});

		return result;
	}

	void Parse()
	{
		Timer t(L"parsing archives");

		auto [genresData, genresIndex] = LoadGenres(m_ini(GENRES, DEFAULT_GENRES));
		m_data.genres                  = std::move(genresData);
		m_genresIndex                  = std::move(genresIndex);
		SetUnknownGenreId();

		for (const auto& inpxFileName : GetInpxFilesInFolder(m_ini))
		{
			PLOGI << QString("parsing %1 started").arg(QString::fromStdWString(inpxFileName));
			const auto inpxContent = ExtractInpxFileNames(inpxFileName);

			const auto zip = [&] {
				if (!exists(inpxFileName))
					return std::unique_ptr<Zip> {};

				const auto fileName = QString::fromStdWString(inpxFileName.generic_wstring());
				return TRY(QString("open %1").arg(fileName), [&] {
					return std::make_unique<Zip>(fileName);
				});
			}();

			if (zip)
				ParseInpxFiles(inpxFileName, zip.get(), inpxContent.inpx);

			PLOGI << QString("parsing %1 finished").arg(QString::fromStdWString(inpxFileName));
		}

		GetFieldList();

		TRY("AddUnIndexedBooks", [this] {
			AddUnIndexedBooks();
			return 0;
		});
		TRY("ScanUnIndexedFolders", [this] {
			ScanUnIndexedFolders();
			return 0;
		});
		TRY("CollectReviews", [this] {
			CollectReviews();
			return 0;
		});
	}

	void CollectReviews()
	{
		const auto reviewsFolder = m_ini(ARCHIVE_FOLDER) / REVIEWS_FOLDER;
		if (!exists(reviewsFolder))
			return;

		PLOGI << "Collect reviews";

		for (const auto& path : std::filesystem::directory_iterator(reviewsFolder) | std::views::filter([](const auto& entry) {
									return !entry.is_directory();
								}) | std::views::transform([](const auto& entry) {
									return entry.path();
								}) | std::views::filter([](const auto& item) {
									return item.extension() == ".7z";
								}))
		{
			auto       fileName = QString::fromStdWString(path);
			const auto zip      = TRY(QString("open %1").arg(fileName), [&] {
                return std::make_unique<Zip>(fileName);
            });
			if (!zip)
				continue;

			for (const auto& file : zip->GetFileNameList())
				m_data.reviews[file.toStdString()].emplace(path.filename());
		}
	}

	void CollectCompilations() const
	{
		const auto compilationsFileName = m_ini(ARCHIVE_FOLDER) / COMPILATIONS;
		if (!exists(compilationsFileName))
			return;

		const auto fileName = QString::fromStdWString(compilationsFileName);
		const auto zip      = TRY(QString("open %1").arg(fileName), [&] {
            return std::make_unique<Zip>(fileName);
        });
		if (!zip)
			return;

		struct Compilation
		{
			size_t                              id;
			size_t                              bookId;
			bool                                covered;
			std::unordered_set<QString>         title;
			std::vector<std::pair<size_t, int>> books;
		};

		std::vector<Compilation> compilations;

		DatabaseWrapper db(m_ini(DB_PATH));

		PLOGI << "collect compilations info";

		sqlite3pp::transaction tr(db);

		sqlite3pp::query query(db, R"(select b.BookID, b.Title
from Books b
join Folders f on f.FolderID = b.FolderID and f.FolderTitle=?
where b.FileName = ? and b.Ext = ?)");

		const auto getBookInfo = [&](const QJsonObject& obj) {
			const auto            folder = obj["folder"].toString().toStdString();
			const auto            file   = obj["file"].toString().toStdWString();
			std::filesystem::path path(file);
			const auto            name = path.stem().string(), ext = path.extension().string();

			query.bind(1, folder.data(), sqlite3pp::nocopy);
			query.bind(2, name.data(), sqlite3pp::nocopy);
			query.bind(3, ext.data(), sqlite3pp::nocopy);

			const auto it = query.begin();

			if (it == query.end())
				return std::make_pair(0LL, QString {});

			const auto bookId = (*it).get<long long>(0);
			auto       title  = QString::fromStdString((*it).get<const char*>(1)).toLower();
			query.reset();

			title.removeIf([&](const QChar ch) {
				return ch != ' ' && ch.category() != QChar::Letter_Lowercase;
			});

			return std::make_pair(bookId, std::move(title));
		};

		const auto doc = QJsonDocument::fromJson(zip->Read(COMPILATIONS_JSON)->GetStream().readAll());
		assert(doc.isArray());
		for (const auto compilationValue : doc.array())
		{
			assert(compilationValue.isObject());
			const auto compilationObject = compilationValue.toObject();
			const auto covered           = compilationObject["covered"].toBool();

			const auto bookId = getBookInfo(compilationObject).first;
			if (!bookId)
				continue;

			auto& compilation = compilations.emplace_back(GetId(), bookId, covered);
			auto& books       = compilation.books;

			for (const auto bookValue : compilationObject["compilation"].toArray())
			{
				assert(bookValue.isObject());
				const auto bookObject  = bookValue.toObject();
				const auto [id, title] = getBookInfo(bookObject);
				if (!id)
					continue;

				books.emplace_back(id, bookObject["part"].toInt());
				std::ranges::copy(
					title.split(' ') | std::views::filter([](const auto& s) {
						return s.length() > 2;
					}),
					std::inserter(compilation.title, compilation.title.end())
				);
			}

			if (books.empty() || compilation.title.empty())
				compilations.pop_back();
		}

		PLOGI << "write compilations info";

		sqlite3pp::command(db, "delete from Compilations_Search").execute();
		sqlite3pp::command(db, "delete from Compilations").execute();

		{
			sqlite3pp::command command(db, "insert into Compilations(CompilationID, BookId, Title, Covered) values(?, ?, ?, ?)");
			for (const auto& compilation : compilations)
			{
				assert(!compilation.title.empty());
				auto title = compilation.title.begin()->toStdString();
				for (const auto& token : compilation.title | std::views::drop(1))
					title.append(" ").append(token.toStdString());

				command.bind(1, compilation.id);
				command.bind(2, compilation.bookId);
				command.bind(3, title, sqlite3pp::nocopy);
				command.bind(4, compilation.covered ? 1 : 0);
				command.execute();
				command.reset();
			}
		}
		{
			sqlite3pp::command command(db, "insert into Compilation_List(CompilationID, BookId, Part) values(?, ?, ?)");
			for (const auto& compilation : compilations)
			{
				for (const auto bookId : compilation.books)
				{
					command.bind(1, compilation.id);
					command.bind(2, bookId.first);
					command.bind(3, bookId.second);
					command.execute();
					command.reset();
				}
			}
		}

		sqlite3pp::command(db, "INSERT INTO Compilations_Search(Compilations_Search) VALUES('rebuild')").execute();

		tr.commit();
	}

	void AddUnIndexedBooks()
	{
		if (!(m_mode & CreateCollectionMode::AddUnIndexedFiles))
			return;

		for (auto& [folder, files] : m_foldersContent)
		{
			if (IsOneOf(folder, REVIEWS_FOLDER, AUTHORS_FOLDER))
				continue;

			std::erase_if(files, [](const auto& item) {
				return item.second.second != 0;
			});
			if (files.empty())
				continue;

			QFileInfo  archiveFileInfo(QString::fromStdWString(m_ini(ARCHIVE_FOLDER) / folder));
			const auto archiveFileName = archiveFileInfo.filePath();
			const auto zip             = TRY(QString("open %1").arg(archiveFileName), [&] {
                return std::make_unique<Zip>(archiveFileName);
            });
			if (!zip)
				continue;

			for (const auto& fileName : files | std::views::keys)
			{
				PLOGW << "Book is not indexed: " << ToMultiByte(folder) << "/" << fileName;
				const auto fileNameStr = QString::fromStdWString(fileName);
				TRY(QString("parse %1").arg(fileNameStr), [&] {
					return ParseFile(folder, *zip, fileNameStr, archiveFileInfo.birthTime());
				});
			}
		}
	}

	void ScanUnIndexedFolders()
	{
		if (!(m_mode & CreateCollectionMode::ScanUnIndexedFolders))
			return;

		const auto inpxFolder = m_ini(ARCHIVE_FOLDER);
		auto       folders    = TRY(std::format("iterate {}", inpxFolder.generic_string()), [&] {
            std::vector<std::wstring> result;
            std::ranges::move(
                std::filesystem::recursive_directory_iterator(inpxFolder) | std::views::filter([](const auto& item) {
                    return !item.is_directory();
                }) | std::views::transform([&](const auto& item) {
                    auto folder = item.path().wstring();
                    folder.erase(0, inpxFolder.string().size() + 1);
                    return folder;
                }) | std::views::filter([&](const auto& item) {
                    static constexpr std::wstring_view specials[] { L"authors", L"covers", L"images", L"reviews" };
                    return !m_foldersContent.contains(item) && std::ranges::none_of(specials, [&](const auto& specialFolder) {
                        return item.starts_with(specialFolder);
                    });
                }),
                std::back_inserter(result)
            );
            return result;
        });
		std::ranges::for_each(std::move(folders), [this](auto&& item) {
			m_foldersToParse.push(std::forward<std::wstring>(item));
		});

		TRY("scan archives", [this] {
			ScanUnIndexedFoldersImpl();
			return true;
		});
	}

	void ScanUnIndexedFoldersImpl()
	{
		if (m_foldersToParse.empty())
			return;

		const auto cpuCount       = static_cast<int>(std::thread::hardware_concurrency());
		const auto maxThreadCount = std::max(std::min(cpuCount - 2, static_cast<int>(m_foldersToParse.size())), 1);
		std::generate_n(std::back_inserter(m_threads), maxThreadCount, [&] {
			return std::make_unique<Thread>(*this);
		});

		while (true)
		{
			std::unique_lock lock(m_foldersToParseGuard);
			m_foldersToParseCondition.wait(lock, [&] {
				return m_foldersToParse.empty();
			});

			if (m_foldersToParse.empty())
				break;
		}

		m_threads.clear();
	}

	void ParseInpxFiles(const Path& inpxFileName, const Zip* zipInpx, const std::vector<std::wstring>& inpxFiles)
	{
		[[maybe_unused]] const auto* r = std::setlocale(LC_ALL, "en_US.utf8"); // NOLINT(concurrency-mt-unsafe)

		if (zipInpx)
		{
			GetFieldList(zipInpx);
			for (const auto& fileName : inpxFiles)
				GetDecodedStream(*zipInpx, fileName, [&](QIODevice& zipDecodedStream) {
					ProcessInpx(inpxFileName, zipDecodedStream, m_ini(ARCHIVE_FOLDER), fileName);
				});

			PLOGI << m_n << " rows parsed";
		}
	}

	void GetFieldList(const Zip* zip = nullptr)
	{
		const auto fieldList = [&]() -> QString {
			return zip && zip->GetFileNameList().contains(STRUCTURE_INFO) ? QString::fromUtf8(zip->Read(STRUCTURE_INFO)->GetStream().readAll()).simplified() : QString(INP_FIELDS_DESCRIPTION);
		}();

		const std::unordered_map<QString, BookBufFieldGetter> bookBufMapping {
#define BOOK_BUF_FIELD_ITEM(NAME) { QString(#NAME), &Get##NAME },
			BOOK_BUF_FIELD_ITEMS_XMACRO
#undef BOOK_BUF_FIELD_ITEM
		};

		auto fields = fieldList.split(';', Qt::SkipEmptyParts);
		m_bookBufMapping.clear();
		m_bookBufMapping.reserve(fields.size());
		std::ranges::transform(fields, std::back_inserter(m_bookBufMapping), [&](const auto& str) {
			const auto it = bookBufMapping.find(str.simplified());
			if (it == bookBufMapping.end())
			{
				PLOGF << "unexpected field name " << str;
				throw std::runtime_error("unexpected field name");
			}

			return it->second;
		});
	}

	void ProcessInpx(const Path& inpxFileName, QIODevice& stream, const Path& rootFolder, std::wstring folder)
	{
		auto&       checkSum      = m_data.inpxFolders[std::make_pair(inpxFileName.filename().wstring(), folder)];
		const auto  mask          = QString::fromStdWString(Path(folder).replace_extension("*"));
		QStringList suitableFiles = QDir(QString::fromStdWString(rootFolder)).entryList({ mask });
		std::ranges::transform(suitableFiles, suitableFiles.begin(), [](const auto& file) {
			return file.toLower();
		});

		folder = suitableFiles.isEmpty() ? Path(folder).replace_extension(m_ini(DEFAULT_ARCHIVE_TYPE)).wstring() : suitableFiles.front().toStdWString();

		QCryptographicHash hash(QCryptographicHash::Md5);
		for (auto byteArray = stream.readLine(); !byteArray.isEmpty(); byteArray = stream.readLine())
		{
			ProcessInpxString(rootFolder, folder, byteArray);
			hash.addData(byteArray);
		}

		checkSum = QString::fromUtf8(hash.result().toHex()).toUpper().toStdString();
	}

	auto& GetFileList(const Path& rootFolder, const std::wstring& folder)
	{
		const auto [it, inserted] = m_foldersContent.try_emplace(folder, std::unordered_map<std::wstring, std::pair<size_t, int>, CaseInsensitiveHash<std::wstring>> {});
		auto& fileList            = it->second;
		if (!inserted)
			return fileList;

		const QFileInfo archiveFileInfo(QString::fromStdWString(rootFolder / folder));
		if (!archiveFileInfo.exists())
			return fileList;

		Zip archiveFile(archiveFileInfo.filePath());
		std::ranges::transform(archiveFile.GetFileNameList(), std::inserter(fileList, fileList.end()), [&](const auto& item) {
			return std::make_pair(item.toStdWString(), std::make_pair(archiveFile.GetFileSize(item), 0));
		});

		return fileList;
	}

	void ProcessInpxString(const Path& rootFolder, const std::wstring& folder, const QByteArray& byteArray)
	{
		auto       line = ToWide(byteArray.constData());
		const auto buf  = ParseBook(folder, line, m_bookBufMapping);

		auto&      fileList = GetFileList(rootFolder, std::wstring(buf.FOLDER));
		const auto fileName = std::wstring(buf.FILE).append(L".").append(buf.EXT);
		const auto it       = fileList.find(fileName);
		const auto found    = it != fileList.end();

		if (!found && !!(m_mode & CreateCollectionMode::SkipLostBooks))
		{
			PLOGW << "'" << QString::fromStdWString(std::wstring(buf.TITLE)) << "'" << " skipped because its file " << QString::fromStdWString(folder) << "/" << ToMultiByte(buf.FILE) << "."
				  << ToMultiByte(buf.EXT) << " not found.";
			return;
		}

		const auto bookIndex = AddBook(buf);
		if (found)
		{
			++it->second.second;
			if (bookIndex < m_data.books.size())
				m_data.books[bookIndex].size = it->second.first;
		}
	}

	bool ParseFile(const std::wstring& folder, const Zip& zip, const QString& fileName, const QDateTime& zipDateTime)
	{
		auto line = Fb2InpxParser::Parse(QString::fromStdWString(folder), zip, fileName, zipDateTime, !!(m_mode & CreateCollectionMode::MarkUnIndexedFilesAsDeleted)).toStdWString();
		if (line.empty())
			return false;

		const auto buf = ParseBook(folder, line, m_bookBufMapping);

		std::lock_guard lock(m_dataGuard);
		AddBook(buf);
		return true;
	}

	size_t ParseDate(const std::wstring_view date, Data& data)
	{
		const auto createUpdate = [this](const int title, const size_t parentId) {
			const auto [it, ok] = m_newUpdates.emplace(GetId());
			assert(ok);
			return Update { *it, title, parentId, {} };
		};

		if (date.empty())
		{
			constexpr auto noDateTitle = std::numeric_limits<int>::max();
			const auto     it          = data.updates.children.find(noDateTitle);
			auto&          update      = it != data.updates.children.end() ? it->second : data.updates.children.try_emplace(noDateTitle, createUpdate(noDateTitle, 0)).first->second;
			return update.id;
		}

		auto       itDate  = std::cbegin(date);
		const auto endDate = std::cend(date);
		const auto year    = std::stoi(Next(itDate, endDate, DATE_SEPARATOR).data());
		const auto month   = std::stoi(Next(itDate, endDate, DATE_SEPARATOR).data());

		const auto itYear      = data.updates.children.find(year);
		auto&      yearUpdate  = itYear != data.updates.children.end() ? itYear->second : data.updates.children.try_emplace(year, createUpdate(year, 0)).first->second;
		const auto itMonth     = yearUpdate.children.find(month);
		auto&      monthUpdate = itMonth != yearUpdate.children.end() ? itMonth->second : yearUpdate.children.try_emplace(month, createUpdate(month, yearUpdate.id)).first->second;
		return monthUpdate.id;
	}

	size_t AddBook(const BookBuf& buf)
	{
		const auto id       = GetId();
		auto       file     = ToMultiByte(buf.FILE) + "." + ToMultiByte(buf.EXT);
		auto&      idFolder = m_data.bookFolders[std::wstring(buf.FOLDER)];
		if (idFolder == 0)
			idFolder = GetId();

		const auto seriesId = Add<int, -1>(buf.SERIES, m_data.series);
		const auto serNo    = To<int>(buf.SERNO, -1);

		{
			const auto [it, inserted] = m_uniqueFiles.try_emplace(std::make_pair(idFolder, file), id);
			if (seriesId != -1)
				m_data.booksSeries[it->second].emplace_back(seriesId, IsOneOf(serNo, 0, -1) ? std::nullopt : std::optional(serNo));
			if (!inserted)
				return std::numeric_limits<size_t>::max();
		}

		auto authorIds = ParseItem(buf.AUTHOR, m_data.authors, Fb2InpxParser::LIST_SEPARATOR, &ParseCheckerAuthor);
		if (authorIds.empty())
			authorIds = ParseItem(std::wstring(AUTHOR_UNKNOWN), m_data.authors);
		assert(!authorIds.empty() && "a book cannot be an orphan");
		std::ranges::copy(authorIds, std::back_inserter(m_data.booksAuthors[id]));

		auto idGenres = ParseItem(
			buf.GENRE,
			m_genresIndex,
			Fb2InpxParser::LIST_SEPARATOR,
			&ParseCheckerDefault,
			[&, &data = m_data.genres](std::wstring_view newItemTitle) {
				const auto result       = std::size(data);
				auto&      genre        = data.emplace_back(newItemTitle, L"", newItemTitle, m_unknownGenreId);
				auto&      unknownGenre = data[m_unknownGenreId];
				genre.dbCode            = ToWide(std::format("{0}.{1}", ToMultiByte(unknownGenre.dbCode), ++unknownGenre.childrenCount));
				m_unknownGenres.push_back(genre.name);
				return result;
			},
			[&data = m_data.genres](const Dictionary& container, std::wstring_view value) {
				const auto itGenre = container.find(value);
				return itGenre != container.end() ? itGenre : std::ranges::find_if(container, [value, &data](const auto& item) {
					return IsStringEqual(value, data[item.second].name);
				});
			}
		);

		const auto updateId = ParseDate(buf.DATE, m_data);

		std::ranges::copy(idGenres, std::back_inserter(m_data.booksGenres[id]));

		if (!buf.KEYWORDS.empty())
			std::ranges::copy(ParseKeywords(buf.KEYWORDS, m_data.keywords, m_uniqueKeywords), std::back_inserter(m_data.booksKeywords[id]));

		auto& book = m_data.books.emplace_back(
			id,
			buf.LIBID,
			buf.TITLE,
			seriesId,
			serNo,
			buf.DATE,
			To<int>(buf.LIBRATE),
			buf.LANG,
			idFolder,
			buf.FILE,
			To<size_t>(buf.INSNO),
			buf.EXT,
			To<size_t>(buf.SIZE),
			To<bool>(buf.DEL, false),
			updateId,
			To<int>(buf.YEAR, -1),
			buf.SOURCELIB
		);

		book.language = m_languageMapping.GetLang(book.language);

		PLOGI_IF((++m_n % LOG_INTERVAL) == 0) << m_n << " books added";

		return m_data.books.size() - 1;
	}

	void LogErrors() const
	{
		if (!std::empty(m_unknownGenres))
		{
			PLOGW << "Unknown genres:";
			for (const auto& genre : m_unknownGenres)
				PLOGW << genre;
		}
	}

private:
	const Ini                  m_ini;
	const CreateCollectionMode m_mode;
	const Callback             m_callback;
	std::unique_ptr<IExecutor> m_executor;

	Data                                                                                                                                                             m_data;
	Dictionary                                                                                                                                                       m_genresIndex;
	size_t                                                                                                                                                           m_n { 0 };
	std::atomic_uint64_t                                                                                                                                             m_parsedN { 0 };
	std::vector<std::wstring>                                                                                                                                        m_unknownGenres;
	size_t                                                                                                                                                           m_unknownGenreId { 0 };
	std::unordered_set<size_t>                                                                                                                                       m_newUpdates;
	std::unordered_map<QString, std::wstring>                                                                                                                        m_uniqueKeywords;
	std::unordered_map<std::pair<size_t, std::string>, size_t, PairHash<size_t, std::string>>                                                                        m_uniqueFiles;
	LanguageMapping                                                                                                                                                  m_languageMapping;
	std::unordered_map<std::wstring, std::unordered_map<std::wstring, std::pair<size_t, int>, CaseInsensitiveHash<std::wstring>>, CaseInsensitiveHash<std::wstring>> m_foldersContent;
	bool                                                                                                                                                             m_oldDataUpdateFound { false };

	BookBufMapping m_bookBufMapping;

	std::queue<std::wstring> m_foldersToParse;
	std::mutex               m_foldersToParseGuard;
	std::condition_variable  m_foldersToParseCondition;

	std::mutex m_dataGuard;

	std::vector<std::unique_ptr<Thread>> m_threads;
};

Parser::Parser()  = default;
Parser::~Parser() = default;

void Parser::CreateNewCollection(IniMap data, const CreateCollectionMode mode, Callback callback)
{
	auto process = [&] {
		PLOGI << "mode: " << static_cast<int>(mode);
		std::make_unique<Impl>(Ini(std::move(data)), mode, std::move(callback)).swap(m_impl);
		m_impl->Process();
		return 0;
	};

	TRY("create collection", process);
}

void Parser::UpdateCollection(IniMap data, const CreateCollectionMode mode, Callback callback)
{
	auto process = [&] {
		PLOGI << "mode: " << static_cast<int>(mode);
		std::make_unique<Impl>(Ini(std::move(data)), mode, std::move(callback)).swap(m_impl);
		m_impl->UpdateDatabase(Impl::GetNewInpxInpxFilesParser());
		return 0;
	};

	TRY("create collection", process);
}

void Parser::RescanCollection(IniMap data, CreateCollectionMode mode, Callback callback)
{
	auto process = [&] {
		PLOGI << "mode: " << static_cast<int>(mode);
		std::make_unique<Impl>(Ini(std::move(data)), mode, std::move(callback)).swap(m_impl);
		m_impl->UpdateDatabase(Impl::GetNewArchivesParser());
		return 0;
	};

	TRY("rescan collection folder", process);
}

void Parser::FillInpx(IniMap data, DB::ITransaction& transaction)
{
	auto process = [&, data = std::move(data)] {
		const Ini  ini(std::move(data));
		const auto folders = GetInpxFolder(ini, true);

		const auto command = transaction.CreateCommand("INSERT INTO Inpx (Folder, File, Hash) VALUES (?, ?, ?)");
		for (const auto& [first, second] : folders)
		{
			const auto folder = ToMultiByte(first.first);
			const auto file   = ToMultiByte(first.second);
			command->Bind(0, folder);
			command->Bind(1, file);
			command->Bind(2, second);
			command->Execute();
		}

		return 0;
	};

	TRY("fill inpx", process);
}

bool Parser::CheckForUpdate(IniMap data, DB::IDatabase& database)
{
	auto process = [&, data = std::move(data)] {
		const Ini  ini(std::move(data));
		const auto inpxFolders = GetInpxFolder(ini, false);

		InpxFolders dbFolders;
		const auto  query = database.CreateQuery("select Folder, File, Hash from Inpx");
		for (query->Execute(); !query->Eof(); query->Next())
			dbFolders.try_emplace(std::make_pair(ToWide(query->Get<const char*>(0)), ToWide(query->Get<const char*>(1))), query->Get<const char*>(2));

		InpxFolders diff;
		const auto  proj = [](const auto& item) {
            return item.first;
		};
		std::ranges::set_difference(inpxFolders, dbFolders, std::inserter(diff, diff.end()), {}, proj, proj);
		return !diff.empty();
	};

	return TRY("fill inpx", process);
}
