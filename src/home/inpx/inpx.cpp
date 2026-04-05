#include "inpx.h"

#include <QCryptographicHash>

#include <fstream>
#include <future>
#include <queue>
#include <ranges>
#include <set>
#include <sstream>

#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>

#include <boost/iterator/iterator_facade.hpp>

#include "fnd/IsOneOf.h"
#include "fnd/StrUtil.h"
#include "fnd/try.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "database/factory/Factory.h"
#include "util/Fb2InpxParser.h"
#include "util/IExecutor.h"
#include "util/executor/factory.h"
#include "util/language.h"
#include "util/timer.h"
#include "util/xml/SaxParser.h"
#include "util/xml/XmlAttributes.h"

#include "Constant.h"
#include "InpxConstant.h"
#include "log.h"
#include "types.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Inpx;
using namespace Util;

namespace
{

constexpr auto INVALID_INDEX = std::numeric_limits<size_t>::max();

const QString UNKNOWN_ROOT        = "unknown_root";
const QString AUTHOR_UNKNOWN_STR  = AUTHOR_UNKNOWN;
const QString GENRE_NOT_SPECIFIED = "unordered:";
size_t        g_id                = 0;

class Dictionary
{
	using DataStorage = std::unordered_map<QString, size_t, WStringHash, WStringEqualTo>;
	using ViewStorage = std::unordered_map<QStringView, size_t, WStringHash, WStringEqualTo>;

public:
	using value_type = std::pair<QStringView, size_t>;

public:
	class const_iterator : public boost::iterator_facade<const_iterator, value_type, boost::forward_traversal_tag>
	{
	public:
		const_iterator(const DataStorage& dataStorage, const ViewStorage& viewStorage, const DataStorage::const_iterator& dataIt, const ViewStorage::const_iterator& viewIt)
			: m_dataStorage { &dataStorage }
			, m_viewStorage { &viewStorage }
			, m_dataIt { dataIt }
			, m_viewIt { viewIt }
		{
			UpdateValue();
		}

		template <class OtherValue>
		explicit const_iterator(const OtherValue& other)
			: m_dataStorage { other.dataStorage }
			, m_viewStorage { other.viewStorage }
			, m_dataIt { other.dataIt }
			, m_viewIt { other.viewIt }
		{
			UpdateValue();
		}

	private:
		friend class boost::iterator_core_access;

		template <class OtherValue>
		bool equal(const OtherValue& other) const
		{
			return this->m_dataIt == other.m_dataIt && this->m_viewIt == other.m_viewIt;
		}

		void increment()
		{
			if (m_dataIt != m_dataStorage->end())
			{
				++m_dataIt;
				if (m_dataIt == m_dataStorage->end())
					m_viewIt = m_viewStorage->begin();
			}
			else
			{
				++m_viewIt;
			}

			UpdateValue();
		}

		value_type& dereference() const
		{
			return m_value;
		}

		void UpdateValue() const
		{
			if (m_dataIt != m_dataStorage->end())
			{
				m_value.first  = m_dataIt->first;
				m_value.second = m_dataIt->second;
			}
			else if (m_viewIt != m_viewStorage->end())
			{
				m_value.first  = m_viewIt->first;
				m_value.second = m_viewIt->second;
			}
		}

	private:
		const DataStorage* m_dataStorage { nullptr };
		const ViewStorage* m_viewStorage { nullptr };

		DataStorage::const_iterator m_dataIt;
		ViewStorage::const_iterator m_viewIt;

		mutable value_type m_value;
	};

public:
	std::pair<QStringView, size_t> emplace(QString key, const size_t value)
	{
		if (const auto it = m_view.find(key); it != m_view.end())
			return std::make_pair(it->first, it->second);

		const auto it = m_data.try_emplace(std::move(key), value).first;
		return std::make_pair(it->first, it->second);
	}

	std::pair<QStringView, size_t> emplace(const QStringView key, const size_t value)
	{
		if (const auto it = m_data.find(key); it != m_data.end())
			return std::make_pair(it->first, it->second);

		const auto it = m_view.try_emplace(key, value).first;
		return std::make_pair(it->first, it->second);
	}

	bool contains(const QStringView key) const
	{
		return m_view.contains(key) || m_data.contains(key);
	}

	std::optional<size_t> find(const QStringView key) const
	{
		if (const auto it = m_view.find(key); it != m_view.end())
			return it->second;

		if (const auto it = m_data.find(key); it != m_data.end())
			return it->second;

		return std::nullopt;
	}

	size_t size() const noexcept
	{
		return m_data.size() + m_view.size();
	}

	template <typename F>
	std::optional<size_t> find_if(const F& f) const
	{
		if (const auto it = std::ranges::find_if(m_data, f); it != m_data.end())
			return it->second;

		if (const auto it = std::ranges::find_if(m_view, f); it != m_view.end())
			return it->second;

		return std::nullopt;
	}

	size_t erase(const QStringView key)
	{
		const auto it = m_data.find(key);
		return static_cast<size_t>(it != m_data.end() ? (m_data.erase(it), 1) : 0) + m_view.erase(key);
	}

	const_iterator cbegin() const
	{
		return { m_data, m_view, m_data.begin(), m_view.begin() };
	}

	const_iterator cend() const
	{
		return { m_data, m_view, m_data.cend(), m_view.cend() };
	}

private:
	DataStorage m_data;
	ViewStorage m_view;
};

Dictionary::const_iterator begin(const Dictionary& dictionary)
{
	return dictionary.cbegin();
}

Dictionary::const_iterator end(const Dictionary& dictionary)
{
	return dictionary.cend();
}

using Books  = std::vector<Book>;
using Genres = std::vector<Genre>;
using Links  = std::unordered_map<size_t, std::vector<size_t>>;

using GetIdFunctor = std::function<size_t(QStringView)>;
using ParseChecker = std::function<bool(QStringView)>;
using Splitter     = std::function<std::vector<QString>(QStringView)>;
using InpxFolders  = std::map<std::pair<QString, QString>, QString, CaseInsensitiveComparer<>>;
using BooksSeries  = std::unordered_map<size_t, std::vector<std::pair<size_t, std::optional<int>>>>;
using Reviews      = std::map<QString, std::set<QString>>;
using Annotations  = std::vector<std::pair<size_t, QString>>;
using FindFunctor  = std::function<std::optional<size_t>(const Dictionary&, QStringView)>;

struct Data
{
	Books       books;
	Dictionary  authors, series, keywords, bookFolders;
	Genres      genres;
	Update      updates;
	Links       booksAuthors, booksGenres, booksKeywords;
	InpxFolders inpxFolders;
	BooksSeries booksSeries;
	Reviews     reviews;
	Annotations annotations;
};

size_t GetId()
{
	return ++g_id;
}

size_t GetIdDefault(QStringView)
{
	return GetId();
}

bool ParseCheckerDefault(QStringView)
{
	return true;
}

bool ParseCheckerAuthor(const QStringView str)
{
	return !str.empty() && !std::ranges::all_of(str, [](const auto ch) {
		return ch == ',';
	});
}

std::optional<size_t> FindDefault(const Dictionary& container, const QStringView value)
{
	return container.find(value);
}

bool IsComment(const QStringView line)
{
	return std::size(line) < 3 || line.startsWith(COMMENT_START);
}

class Ini
{
public:
	explicit Ini(Parser::IniMap data)
		: _data(std::move(data))
	{
	}

	const QString& operator()(const QString& key, const QString& defaultValue = {}) const
	{
		const auto it = _data.find(key);
		if (it != _data.end())
			return it->second;

		if (!defaultValue.isEmpty())
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

class AnnotationsParser final : public SaxParser
{
	static constexpr auto FOLDER = "folder";
	static constexpr auto FILE   = "file";

public:
	static QString Prepare(QStringList annotation)
	{
		QStringList list;
		for (auto&& str : annotation)
		{
			str = str.toLower();
			std::ranges::transform(str, std::begin(str), [&](const QChar& ch) {
				const auto category = ch.category();
				if (IsOneOf(category, QChar::Separator_Space, QChar::Separator_Line, QChar::Separator_Paragraph, QChar::Other_Control)
				    || (category >= QChar::Punctuation_Connector && category <= QChar::Punctuation_Other))
					return QChar { 0x20 };

				return ch;
			});
			str.removeIf([](const QChar ch) {
				return ch != ' ' && !IsOneOf(ch.category(), QChar::Number_DecimalDigit, QChar::Letter_Lowercase);
			});

			for (auto&& word : str.split(' ', Qt::SkipEmptyParts))
				if (word.length() > 2)
					list << std::move(word);
		}

		auto result = list.join(' ');
		if (result.size() > 10240)
			result.resize(10240);

		return result;
	}

public:
	AnnotationsParser(QIODevice& stream, DB::ICommand& command, const std::unordered_map<QString, long long>& books)
		: SaxParser(stream)
		, m_command { command }
		, m_books { books }
	{
	}

private: // SaxParser
	bool OnStartElement(const QString& name, const QString&, const XmlAttributes& attributes) override
	{
		if (name == FOLDER)
		{
			m_folder = attributes.GetAttribute("name");
			PLOGD << "load annotations " << m_folder;
		}
		else if (name == FILE)
		{
			m_file = attributes.GetAttribute("name");
		}

		return true;
	}

	bool OnEndElement(const QString& name, const QString&) override
	{
		if (name != FILE)
			return true;

		const auto annotation = Prepare(std::move(m_annotation));
		const auto it         = m_books.find(QString("%1/%2").arg(m_folder, m_file));
		if (it == m_books.end())
			return true;

		m_command.Bind(0, it->second);
		m_command.Bind(1, annotation);

		m_command.Execute();

		m_annotation = {};

		return true;
	}

	bool OnCharacters(const QString&, const QString& value) override
	{
		m_annotation << value;
		return true;
	}

private:
	DB::ICommand&                                 m_command;
	const std::unordered_map<QString, long long>& m_books;

	QString     m_folder;
	QString     m_file;
	QStringList m_annotation;
};

QStringView QNext(QString::const_iterator& beg, const QString::const_iterator end, const char separator)
{
	beg              = std::find_if(beg, end, [](const QChar c) {
		return c.category() != QChar::Separator_Space;
	});
	auto        next = std::find(beg, end, separator);
	QStringView result(beg, next);
	beg = next != end ? std::next(next) : end;
	return result;
}

auto LoadGenres(const QString& genresIniFileName)
{
	Genres     genres;
	Dictionary index;

	QFile stream(genresIniFileName);
	if (!stream.open(QIODevice::ReadOnly))
		throw std::invalid_argument(std::format("Cannot open '{}'", genresIniFileName));

	genres.emplace_back("");
	index.emplace(genres.front().code, size_t { 0 });

	for (auto byteArray = stream.readLine(); !byteArray.isEmpty(); byteArray = stream.readLine())
	{
		auto line = QString::fromUtf8(byteArray);
		if (IsComment(line))
			continue;

		auto it     = std::cbegin(line);
		auto codes  = QNext(it, std::cend(line), GENRE_SEPARATOR).toString();
		auto itCode = std::cbegin(codes);
		auto code   = QNext(itCode, std::cend(codes), Fb2InpxParser::LIST_SEPARATOR).toString();
		index.emplace(code, std::size(genres));

		while (itCode != std::cend(codes))
			index.emplace(QNext(itCode, std::cend(codes), Fb2InpxParser::LIST_SEPARATOR).toString(), std::size(genres));

		const auto parent = QNext(it, std::end(line), GENRE_SEPARATOR);
		const auto title  = QNext(it, std::end(line), GENRE_SEPARATOR);
		genres.emplace_back(std::move(code), parent.toString(), title.toString());
	}

	std::for_each(std::next(std::begin(genres)), std::end(genres), [&index, &genres](Genre& genre) {
		if (const auto it = index.find(genre.parentCode))
		{
			auto& parent   = genres[*it];
			genre.parentId = *it;
			genre.dbCode   = QString("%1%2").arg(parent.dbCode.isEmpty() ? QString {} : parent.dbCode + '.').arg(++parent.childrenCount, 3, 10, QChar { '0' });
		}
		else
		{
			assert(false && "unexpected parentCode");
		}
	});

	return std::make_pair(std::move(genres), std::move(index));
}

template <typename ValueType, typename ReturnType, ReturnType emptyValue = 0>
ReturnType Add(ValueType value, Dictionary& container, const GetIdFunctor& getId = &GetIdDefault, const FindFunctor& find = &FindDefault)
{
	if (value.isEmpty())
		return emptyValue;

	auto it = find(container, value);
	if (!it)
		it = container.emplace(value, getId(value)).second;

	return static_cast<ReturnType>(*it);
}

std::vector<size_t> ParseItem(
	const QStringView   data,
	Dictionary&         container,
	const char          separator    = Fb2InpxParser::LIST_SEPARATOR,
	const ParseChecker& parseChecker = &ParseCheckerDefault,
	const GetIdFunctor& getId        = &GetIdDefault,
	const FindFunctor&  find         = &FindDefault
)
{
	std::unordered_set<size_t> unique;
	std::vector<size_t>        result;
	auto                       it = std::ranges::find_if(data, [=](const auto ch) {
		return ch != separator;
	});
	while (it != std::cend(data))
		if (const auto value = QNext(it, std::cend(data), separator); parseChecker(value))
			if (const auto [valueIt, added] = unique.insert(Add<QStringView, size_t>(value, container, getId, find)); added)
				result.emplace_back(*valueIt);

	return result;
}

std::vector<size_t> ParseItem(const QStringView data, Dictionary& container, const Splitter& splitter, const GetIdFunctor& getId = &GetIdDefault, const FindFunctor& find = &FindDefault)
{
	std::unordered_set<size_t> unique;
	std::vector<size_t>        result;
	for (auto&& value : splitter(data))
		if (const auto [valueIt, added] = unique.insert(Add<QString, size_t>(std::move(value), container, getId, find)); added)
			result.emplace_back(*valueIt);

	return result;
}

std::vector<size_t> ParseKeywords(const QStringView keywordsSrc, Dictionary& keywordsLinks)
{
	return ParseItem(
		keywordsSrc,
		keywordsLinks,
		[&](const QStringView item) {
			std::vector<QString> keywordsList;
			auto                 keywordsStr =
				item.toString().replace("--", ",").replace(" - ", ",").replace(" & ", " and ").replace(QChar(L'\x0401'), QChar(L'\x0415')).replace(QChar(L'\x0451'), QChar(L'\x0435')).replace("_", " ");
			keywordsStr.remove(QRegularExpression(QString(R"([@!\?"%1%2])").arg(QChar(L'\x00ab'), QChar(L'\x00bb'))));
			auto list = keywordsStr.split(QRegularExpression(R"([,;#/\\\.\(\)\[\]])"), Qt::SkipEmptyParts);
			std::ranges::transform(list, list.begin(), [](const auto& str) {
				return str.simplified();
			});

			if (const auto [from, to] = std::ranges::remove_if(
					list,
					[](const auto& str) {
						return str.length() < 3 || str.startsWith("DocId:", Qt::CaseInsensitive);
					}
				);
		        from != to)
				list.erase(from, to);
			assert(std::ranges::none_of(list, [](const auto& str) {
				return str.startsWith("DocId:", Qt::CaseInsensitive);
			}));

			for (const auto& keyword : list)
				for (auto&& word : keyword.split(':', Qt::SkipEmptyParts))
					keywordsList.emplace_back(std::move(word));

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
			if (const auto [from, to] = std::ranges::remove_if(
					keywordsList,
					[](const auto& str) {
						return str.length() < 3;
					}
				);
		        from != to)
				keywordsList.erase(from, to);

			return keywordsList;
		},
		&GetIdDefault
	);
}

using BookBufFieldGetter = QStringView& (*)(Book&);
using BookBufMapping     = std::vector<BookBufFieldGetter>;

#define BOOK_BUF_FIELD_ITEM(NAME)      \
	QStringView& Get##NAME(Book& book) \
	{                                  \
		return book.NAME;              \
	}
BOOK_BUF_FIELD_ITEMS_XMACRO
#undef BOOK_BUF_FIELD_ITEM

QStringView BOOK_BUF_FIELD_DEFAULT_VIEW;

QStringView& GetBookBufFieldDefault(Book&)
{
	return BOOK_BUF_FIELD_DEFAULT_VIEW;
}

Book ParseBook(const QString& line, const BookBufMapping& f, QString folder)
{
	Book buf;

	auto       it  = std::cbegin(line);
	const auto end = std::cend(line);

	for (size_t i = 0, sz = f.size(); i < sz && it != end; ++i)
		f[i](buf) = QNext(it, end, Fb2InpxParser::FIELDS_SEPARATOR);

	buf.fileName = QString("%1.%2").arg(buf.FILE, buf.EXT);

	if (buf.GENRE.size() < 2)
		buf.GENRE = GENRE_NOT_SPECIFIED;

	if (buf.FOLDER.isEmpty())
	{
		buf.folder = std::move(folder);
		buf.FOLDER = buf.folder;
	}

	return buf;
}

struct InpxContent
{
	std::vector<QString> collectionInfo;
	std::vector<QString> versionInfo;
	std::vector<QString> inpx;
};

InpxContent ExtractInpxFileNames(const QString& inpxPath)
{
	if (!QFile::exists(inpxPath))
	{
		PLOGW << inpxPath << " not found";
		return {};
	}

	InpxContent inpxContent;

	const auto zip = TRY(QString("open %1").arg(inpxPath), [&] {
		return std::make_unique<Zip>(inpxPath);
	});
	if (!zip)
		return inpxContent;

	for (auto&& folder : zip->GetFileNameList())
	{
		if (folder == COLLECTION_INFO)
			inpxContent.collectionInfo.emplace_back(std::move(folder));
		else if (folder == VERSION_INFO)
			inpxContent.versionInfo.emplace_back(std::move(folder));
		else if (folder.endsWith(INP_EXT))
			inpxContent.inpx.emplace_back(std::move(folder));
		else
			PLOGI << folder << " skipped";
	}

	PLOGD << QString("%1 content: connection info: %2, version info: %3, inp: %4").arg(inpxPath).arg(inpxContent.collectionInfo.size()).arg(inpxContent.versionInfo.size()).arg(inpxContent.inpx.size());

	return inpxContent;
}

void GetDecodedStream(const Zip& zip, const QString& file, const std::function<void(QIODevice&)>& f)
{
	PLOGI << file;
	TRY("get decoded stream", [&] {
		const auto stream = zip.Read(file);
		f(stream->GetStream());
		return 0;
	});
}

bool ExecuteScript(DB::IDatabase& db, QString action, const QString& scriptFileName)
{
	Timer t(std::move(action));

	QFile stream(scriptFileName);
	if (!stream.open(QIODevice::ReadOnly))
		throw std::runtime_error(std::format("Cannot open {}", scriptFileName));

	QJsonParseError jsonParseError;
	const auto      doc = QJsonDocument::fromJson(stream.readAll(), &jsonParseError);
	if (jsonParseError.error != QJsonParseError::NoError)
		throw std::runtime_error(std::format("Cannot parse {}: {}", scriptFileName, jsonParseError.errorString()));

	if (!doc.isArray())
		throw std::runtime_error(std::format("{} must be json array", scriptFileName));

	const auto getCommand = [&](const QJsonValueRef& queryJsonValue) -> QString {
		if (queryJsonValue.isString())
			return queryJsonValue.toString();

		assert(queryJsonValue.isArray());
		return (queryJsonValue.toArray() | std::views::transform([&](const auto& item) {
					assert(item.isString());
					return item.toString();
				})
		        | std::ranges::to<QStringList>())
		    .join('\n');
	};

	const auto tr = db.CreateTransaction();
	for (const auto queryJsonValue : doc.array())
	{
		const auto command = getCommand(queryJsonValue).toStdString();
		PLOGI << command;
		command.starts_with("PRAGMA") ? tr->CreateQuery(command)->Execute() : tr->CreateCommand(command)->Execute();
	}
	tr->Commit();

	return true;
}

void SetIsNavigationDeleted(DB::IDatabase& db)
{
	const auto tr = db.CreateTransaction();
	for (const auto& [table, where, _, join, additional] : IS_DELETED_UPDATE_ARGS)
	{
		PLOGI << "set IsDeleted for " << table;
		const auto command = QString(IS_DELETED_UPDATE_STATEMENT_TOTAL).arg(table, join, where, additional).toStdString();
#ifndef NDEBUG
		PLOGD << command;
#endif
		tr->CreateCommand(command)->Execute();
	}
	tr->Commit();
}

int Analyze(DB::IDatabase& db)
{
	SetIsNavigationDeleted(db);

	const auto tr = db.CreateTransaction();
	PLOGI << "ANALYZE";

	const auto yearsExist = [&] {
		const auto query = tr->CreateQuery("select exists (select 42 from Books where Year is not null)");
		query->Execute();
		assert(!query->Eof());
		return query->Get<int>(0);
	}();
	[[maybe_unused]] auto ok = tr->CreateCommand(std::format("insert or replace into Settings(SettingID, SettingValue) values(1, '{}')", yearsExist))->Execute();
	assert(ok);

	ok = tr->CreateCommand("ANALYZE")->Execute();
	assert(ok);

	return tr->Commit() ? 0 : 1;
}

void WriteDatabaseVersion(DB::IDatabase& db, const QString& statement)
{
	const auto tr = db.CreateTransaction();
	tr->CreateCommand(statement)->Execute();
	tr->Commit();
}

template <typename Container, typename Functor>
size_t StoreRange(DB::IDatabase& db, const QString& process, const std::string_view query, const Container& container, Functor&& f, const std::string_view queryAfter = {})
{
	auto impl = [&] {
		const auto rowsTotal = std::size(container);
		if (rowsTotal == 0)
			return rowsTotal;

		Timer  t(QString("store %1 %2").arg(process).arg(rowsTotal));
		size_t rowsInserted = 0;

		const auto tr      = db.CreateTransaction();
		const auto command = tr->CreateCommand(query);

		const auto log = [rowsTotal, &rowsInserted] {
			PLOGD << std::format("{0} rows inserted ({1}%)", rowsInserted, rowsInserted * 100 / rowsTotal);
		};

		const auto result =
			std::accumulate(container.cbegin(), container.cend(), static_cast<size_t>(0), [f = std::forward<Functor>(f), &db, &cmd = *command, &rowsInserted, &log](const size_t init, const auto& value) {
				if (!f(cmd, value))
					return init + 1;

				if (++rowsInserted % LOG_INTERVAL == 0)
					log();

				return init;
			});

		log();
		if (rowsTotal != rowsInserted)
		{
			PLOGE << rowsTotal - rowsInserted << " rows lost";
		}

		if (!queryAfter.empty())
			tr->CreateCommand(queryAfter)->Execute();

		{
			Timer tc("commit");
			tr->Commit();
		}

		return result;
	};

	return TRY(process, impl);
}

size_t Store(DB::IDatabase& db, Data& data)
{
	size_t result  = 0;
	result        += StoreRange(
		db,
		"Authors",
		"INSERT INTO Authors (AuthorID, LastName, FirstName, MiddleName, SearchName) VALUES(?, ?, ?, ?, ?)",
		data.authors,
		[](DB::ICommand& cmd, const Dictionary::value_type& item) {
			const auto& [title, id] = item;
			auto       it           = std::cbegin(title);
			const auto last         = QNext(it, std::cend(title), Fb2InpxParser::NAMES_SEPARATOR);
			const auto first        = QNext(it, std::cend(title), Fb2InpxParser::NAMES_SEPARATOR);
			const auto middle       = QNext(it, std::cend(title), Fb2InpxParser::NAMES_SEPARATOR);
			const auto lastUp       = last.toString().toUpper();

			cmd.Bind(0, id);
			cmd.Bind(1, last);
			cmd.Bind(2, first);
			cmd.Bind(3, middle);
			cmd.Bind(4, lastUp);

			return cmd.Execute();
		},
		"INSERT INTO Authors_Search(Authors_Search) VALUES('rebuild')"
	);

	result += StoreRange(
		db,
		"Series",
		"INSERT INTO Series (SeriesID, SeriesTitle, SearchTitle) VALUES (?, ?, ?)",
		data.series,
		[](DB::ICommand& cmd, const Dictionary::value_type& item) {
			const auto& [title, id] = item;
			cmd.Bind(0, id);
			cmd.Bind(1, title);
			cmd.Bind(2, title.toString().toUpper());
			return cmd.Execute();
		},
		"INSERT INTO Series_Search(Series_Search) VALUES('rebuild')"
	);

	result += StoreRange(db, "Folders", "INSERT INTO Folders (FolderID, FolderTitle) VALUES (?, ?)", data.bookFolders, [](DB::ICommand& cmd, const auto& item) {
		const auto& [title, id] = item;
		cmd.Bind(0, id);
		cmd.Bind(1, title);
		return cmd.Execute();
	});

	result += StoreRange(db, "InpxFolders", "INSERT INTO Inpx (Folder, File, Hash) VALUES (?, ?, ?)", data.inpxFolders, [](DB::ICommand& cmd, const auto& item) {
		cmd.Bind(0, item.first.first);
		cmd.Bind(1, item.first.second);
		cmd.Bind(2, item.second);
		return cmd.Execute();
	});

	result += StoreRange(db, "Keywords", "INSERT INTO Keywords (KeywordID, KeywordTitle, SearchTitle) VALUES (?, ?, ?)", data.keywords, [](DB::ICommand& cmd, const Dictionary::value_type& item) {
		const auto& [title, id] = item;
		cmd.Bind(0, id);
		cmd.Bind(1, title);
		cmd.Bind(2, title.toString().toUpper());
		return cmd.Execute();
	});

	std::vector<size_t> newGenresIndex;
	for (size_t i = 0, sz = std::size(data.genres); i < sz; ++i)
		if (data.genres[i].newGenre)
			newGenresIndex.push_back(i);
	result += StoreRange(
		db,
		"Genres",
		"INSERT INTO Genres (GenreCode, ParentCode, FB2Code, GenreAlias) VALUES(?, ?, ?, ?)",
		newGenresIndex | std::views::drop(1),
		[&genres = data.genres](DB::ICommand& cmd, const size_t n) {
			cmd.Bind(0, genres[n].dbCode);
			cmd.Bind(1, genres[genres[n].parentId].dbCode);
			cmd.Bind(2, genres[n].code);
			cmd.Bind(3, genres[n].name);
			return cmd.Execute();
		}
	);

	const char* queryText  = "INSERT INTO Books ("
							 "BookID   , LibID     , Title    , "
							 "UpdateDate, LibRate  , Lang    , Year, "
							 "FolderID , FileName  , Ext     , "
							 "BookSize , IsDeleted, UpdateId, SourceLib, SearchTitle"
							 ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
	result                += StoreRange(
		db,
		"Books",
		queryText,
		data.books,
		[](DB::ICommand& cmd, const Book& book) {
			cmd.Bind(0, book.id);
			cmd.Bind(1, book.LIBID);
			cmd.Bind(2, book.TITLE);
			cmd.Bind(3, book.DATE);
			cmd.Bind(4, book.LIBRATE);
			cmd.Bind(5, book.LANG);
			book.YEAR.isEmpty() ? cmd.Bind(6) : cmd.Bind(6, book.YEAR);
			cmd.Bind(7, book.folderId);
			cmd.Bind(8, book.FILE);
			cmd.Bind(9, QString(".%1").arg(book.EXT));
			cmd.Bind(10, book.SIZE);
			cmd.Bind(11, book.DEL);
			cmd.Bind(12, book.updateId);
			cmd.Bind(13, book.SOURCELIB);
			cmd.Bind(14, book.TITLE.toString().toUpper());
			return cmd.Execute();
		},
		"INSERT INTO Books_Search(Books_Search) VALUES('rebuild')"
	);

	{
		std::unordered_set<QStringView> languages;
		std::ranges::transform(data.books, std::inserter(languages, languages.end()), [](const auto& item) {
			return item.LANG;
		});
		result += StoreRange(db, "Languages", "INSERT OR IGNORE INTO Languages (LanguageCode) VALUES(?)", languages, [](DB::ICommand& cmd, const QStringView& item) {
			cmd.Bind(0, item);
			return cmd.Execute();
		});
	}

	result += StoreRange(db, "Author_List", "INSERT INTO Author_List (AuthorID, BookID, OrdNum) VALUES(?, ?, ?)", data.booksAuthors, [](DB::ICommand& cmd, const Links::value_type& item) {
		return std::accumulate(item.second.cbegin(), item.second.cend(), true, [&, ordNum = 0](const auto init, const size_t id) mutable {
			cmd.Bind(0, id);
			cmd.Bind(1, item.first);
			cmd.Bind(2, ++ordNum);
			return cmd.Execute() && init;
		});
	});

	result += StoreRange(db, "Series_List", "INSERT INTO Series_List (SeriesID, BookID, SeqNumber, OrdNum) VALUES(?, ?, ?, ?)", data.booksSeries, [](DB::ICommand& cmd, const BooksSeries::value_type& item) {
		std::unordered_set<size_t> storedSeries;
		return std::accumulate(item.second.cbegin(), item.second.cend(), true, [&, ordNum = 0](const auto init, const std::pair<size_t, std::optional<int>> series) mutable {
			if (!storedSeries.insert(series.first).second)
				return init;

			cmd.Bind(0, series.first);
			cmd.Bind(1, item.first);
			series.second ? cmd.Bind(2, *series.second) : cmd.Bind(2);
			cmd.Bind(3, ++ordNum);
			return cmd.Execute() && init;
		});
	});

	result += StoreRange(db, "Keyword_List", "INSERT INTO Keyword_List (KeywordID, BookID, OrdNum) VALUES(?, ?, ?)", data.booksKeywords, [](DB::ICommand& cmd, const Links::value_type& item) {
		return std::accumulate(item.second.cbegin(), item.second.cend(), true, [&, ordNum = 0](const auto init, const size_t id) mutable {
			cmd.Bind(0, id);
			cmd.Bind(1, item.first);
			cmd.Bind(2, ++ordNum);
			return cmd.Execute() && init;
		});
	});

	auto genres = data.genres | std::views::transform([](const auto& item) {
					  return item.dbCode;
				  })
	            | std::ranges::to<std::vector>();
	result +=
		StoreRange(db, "Genre_List", "INSERT INTO Genre_List (BookID, GenreCode, OrdNum) VALUES(?, ?, ?)", data.booksGenres, [genres = std::move(genres)](DB::ICommand& cmd, const Links::value_type& item) {
			return std::accumulate(item.second.cbegin(), item.second.cend(), true, [&, ordNum = 0](const auto init, const size_t id) mutable {
				assert(id < std::size(genres));
				cmd.Bind(0, item.first);
				cmd.Bind(1, genres[id]);
				cmd.Bind(2, ++ordNum);
				return cmd.Execute() && init;
			});
		});

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

	result += StoreRange(db, "Updates", "INSERT INTO Updates (UpdateID, UpdateTitle, ParentID) VALUES(?, ?, ?)", updates, [](DB::ICommand& cmd, const auto& item) {
		cmd.Bind(0, std::get<0>(item));
		cmd.Bind(1, std::get<1>(item));
		cmd.Bind(2, std::get<2>(item));
		return cmd.Execute();
	});

	if (!data.reviews.empty())
	{
		std::vector<std::pair<size_t, QString>> reviews;

		{
			const auto query = db.CreateQuery("select b.BookID, f.FolderTitle||'#'||b.FileName||b.Ext from Books b join Folders f on f.FolderID = b.FolderID");
			for (query->Execute(); !query->Eof(); query->Next())
			{
				const auto bookId = query->Get<int64_t>(0);
				const auto uid    = query->Get<QString>(1);
				if (const auto it = data.reviews.find(uid); it != data.reviews.end())
				{
					std::ranges::transform(it->second, std::back_inserter(reviews), [&](const auto& item) {
						return std::make_pair(bookId, item);
					});
				}
			}
		}

		result += StoreRange(db, "Reviews", "INSERT INTO Reviews (BookID, Folder) VALUES(?, ?)", reviews, [](DB::ICommand& cmd, const auto& item) {
			cmd.Bind(0, item.first);
			cmd.Bind(1, item.second);
			return cmd.Execute();
		});
	}

	result += StoreRange(db, "Annotations", "INSERT INTO Annotations (BookID, Text) VALUES(?, ?)", data.annotations, [](DB::ICommand& cmd, const auto& item) {
		cmd.Bind(0, item.first);
		cmd.Bind(1, item.second);
		return cmd.Execute();
	});

	return result;
}

std::vector<QString> GetInpxFilesInFolder(const Ini& ini)
{
	PLOGV << "GetInpxFilesInFolder started";
	if (ini.Exists(INPX_PATH))
		if (const auto explicitInpxPath = ini(INPX_PATH); !explicitInpxPath.isEmpty())
			return { explicitInpxPath };

	std::vector<QString> result;
	for (QDirIterator it(ini(ARCHIVE_FOLDER), { "*.inpx" }, QDir::Files); it.hasNext();)
		result.emplace_back(it.nextFileInfo().filePath());

	PLOGV << "GetInpxFilesInFolder finished";
	return result;
}

InpxFolders GetInpxFolder(const Ini& ini, const bool needHashes)
{
	InpxFolders folders;
	for (const auto& inpxPath : GetInpxFilesInFolder(ini))
	{
		PLOGV << "check " << inpxPath << " started";

		const auto zip = TRY(QString("open %1").arg(inpxPath), [&] {
			return std::make_unique<Zip>(inpxPath);
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
				return QString::fromUtf8(engine.result().toHex()).toUpper();
			}()
			                       : QString();

			folders.try_emplace(std::make_pair(QFileInfo(inpxPath).fileName(), fileName), std::move(hash));
		}

		PLOGV << "check " << inpxPath << " finished";
	}
	return folders;
}

template <typename T>
T ReadDictionary(const std::string_view name, DB::IDatabase& db, const char* statement, const auto& f)
{
	PLOGI << "Read " << name;
	T          data;
	const auto query = db.CreateQuery(statement);
	for (query->Execute(); !query->Eof(); query->Next())
	{
		const auto [key, value] = f(*query);
		data.emplace(key, value);
	}

	return data;
}

Update ReadUpdates(DB::IDatabase& db)
{
	PLOGI << "Read updates";
	Update                              result;
	std::unordered_map<size_t, Update*> updates {
		{ 0, &result },
	};
	const auto query = db.CreateQuery("select UpdateID, UpdateTitle, ParentID from Updates order by ParentId");
	for (query->Execute(); !query->Eof(); query->Next())
	{
		const auto id       = query->Get<long long>(0);
		const auto title    = query->Get<int>(1);
		const auto parentId = query->Get<long long>(2);

		const auto parentIt = updates.find(parentId);
		assert(parentIt != updates.end());
		auto& child = parentIt->second->children.try_emplace(title, Update { static_cast<size_t>(id), title, static_cast<size_t>(parentId) }).first->second;
		updates.try_emplace(id, &child);
	}

	return result;
}

Reviews ReadReviews(DB::IDatabase& db)
{
	PLOGI << "Read reviews";
	Reviews    reviews;
	const auto query = db.CreateQuery("select f.FolderTitle||'#'||b.FileName||b.Ext, r.Folder from Reviews r join Books b on b.BookID = r.BookID join Folders f on f.FolderID = b.FolderID");
	for (query->Execute(); !query->Eof(); query->Next())
	{
		const char* uid    = query->Get<const char*>(0);
		const char* folder = query->Get<const char*>(1);
		reviews[uid].emplace(folder);
	}

	return reviews;
}

std::pair<Genres, Dictionary> ReadGenres(DB::IDatabase& db, const QString& genresFileName)
{
	PLOGI << "Read genres";
	std::pair<Genres, Dictionary> result;
	auto& [genres, index] = result;

	genres.emplace_back("0");
	index.emplace(genres.front().code, size_t { 0 });

	std::map<QString, std::vector<size_t>> children;

	auto       n     = std::size(genres);
	const auto query = db.CreateQuery("select FB2Code, GenreCode, ParentCode, GenreAlias from Genres");
	for (query->Execute(); !query->Eof(); query->Next())
	{
		const auto* fb2Code    = query->Get<const char*>(0);
		const auto* genreCode  = query->Get<const char*>(1);
		const auto* parentCode = query->Get<const char*>(2);
		const auto* genreAlias = query->Get<const char*>(3);

		Genre genre(fb2Code, "", genreAlias);
		genre.dbCode = genreCode;
		children[parentCode].push_back(n);
		index.emplace(genre.code, n++);
		genre.newGenre = false;
		genres.emplace_back(std::move(genre));
	}

	for (size_t i = 0, sz = std::size(genres); i < sz; ++i)
	{
		const auto it = children.find(genres[i].dbCode);
		if (it == children.end())
			continue;

		genres[i].childrenCount = std::size(it->second);
		for (const auto k : it->second)
		{
			genres[k].parentId   = i;
			genres[k].parentCode = genres[i].code;
		}
	}

	assert(n == std::size(genres));

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
		assert(it);
		iniGenres[n].parentId = *it;
	}

	const auto& iniIndexConst = iniIndex;
	for (const auto& [code, k] : iniIndexConst)
	{
		if (index.contains(code))
			continue;

		const auto& genre = iniGenres[k];
		const auto  it    = index.find(genre.code);
		assert(it);
		index.emplace(code.toString(), *it);
	}

	return result;
}

void SetNextId(DB::IDatabase& db)
{
	const auto query = db.CreateQuery(GET_MAX_ID_QUERY);
	query->Execute();
	assert(!query->Eof());
	g_id = query->Get<long long>(0);
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
			QString folder;
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
				const QFileInfo archiveFileInfo(m_ini(ARCHIVE_FOLDER) + "/" + folder);
				const auto      archiveFileName = archiveFileInfo.filePath();
				const auto      zip             = TRY(QString("open %1").arg(archiveFileName), [&]() -> std::unique_ptr<Zip> {
					return std::make_unique<Zip>(archiveFileName);
				});
				if (!zip)
					return -1;

				Work(folder, *zip, zip->GetFileNameList() | std::ranges::to<std::vector>(), archiveFileInfo.birthTime());

				return 0;
			};

			TRY(QString("parsing %1").arg(folder), work);
		}
	}

	void Work(const QString& folder, const Zip& zip, std::vector<QString> zipFileList, const QDateTime& zipDateTime)
	{
		const auto totalCount = zipFileList.size();
		size_t     counter    = 0;

		const auto enumerate = [&](const auto& range, const auto& process) {
			for (const auto& fileName : range)
			{
				++counter;
				PLOGD << "parsing " << folder << "/" << fileName << "  " << counter << " (" << totalCount << ") " << 100 * counter / zipFileList.size() << "%";
				process(fileName);
			}
		};

		auto tail = std::ranges::partition(
			zipFileList,
			[](const QString& item) {
				return item.compare("fb2", Qt::CaseInsensitive) != 0;
			},
			[](const QString& item) {
				return QFileInfo(item).suffix();
			}
		);

		enumerate(tail, [&](const QString& fileName) {
			auto process = [&] {
				return ParseFile(folder, zip, fileName, zipDateTime);
			};
			TRY(QString("parse %1").arg(fileName), process);
		});
		zipFileList.erase(std::begin(tail), std::end(tail));

		tail = std::ranges::partition(
			zipFileList,
			[](const QString& item) {
				return item.compare("fbd", Qt::CaseInsensitive) != 0;
			},
			[](const QString& item) {
				return QFileInfo(item).suffix();
			}
		);

		enumerate(tail, [&](const QString& fileName) {
			auto process = [&] {
				const auto it = std::ranges::find_if(zipFileList.begin(), tail.begin(), [&fileName, completeBaseName = QFileInfo(fileName).completeBaseName()](const QString& item) {
					return fileName.compare(item, Qt::CaseInsensitive) != 0
					    && (completeBaseName.compare(item, Qt::CaseInsensitive) == 0 || completeBaseName.compare(QFileInfo(item).completeBaseName(), Qt::CaseInsensitive) == 0);
				});

				if (it == tail.begin())
					return INVALID_INDEX;

				const QFileInfo fileInfo(*it);
				const auto      index = ParseFile(folder, zip, fileName, zipDateTime, fileInfo.completeBaseName(), fileInfo.suffix());
				if (index == INVALID_INDEX)
					return index;

				it->clear();
				++counter;

				return index;
			};
			TRY(QString("parse %1").arg(fileName), process);
		});
		zipFileList.erase(std::begin(tail), std::end(tail));
		std::erase_if(zipFileList, [](const QString& item) {
			return item.isEmpty();
		});

		enumerate(zipFileList, [&](const QString& fileName) {
			const QFileInfo fileInfo(fileName);
			if (fileInfo.suffix().toLower() != "zip")
				return;

			const Zip  subZip(zip.Read(fileName)->GetStream());
			const auto subZipFiles = subZip.GetFileNameList();
			const auto it          = std::ranges::find_if(subZipFiles, [](const QString& item) {
				return QFileInfo(item).suffix().toLower() == "fbd";
			});
			if (it == subZipFiles.end())
				return;

			if (ParseFile(folder, subZip, *it, zipDateTime, fileInfo.completeBaseName(), fileInfo.suffix()) != INVALID_INDEX)
				++counter;
		});
	}

private:
	void ProcessImpl(bool& ok)
	{
		ok = false;
		Timer t("work");

		const auto& dbFileName       = m_ini(DB_PATH);
		const auto  connectionString = QString("path=%1").arg(dbFileName).toStdString();

		m_db = Create(DB::Factory::Impl::Sqlite, connectionString);

		ExecuteScript(*m_db, "create database", m_ini(DB_CREATE_SCRIPT, DEFAULT_DB_CREATE_SCRIPT));
		WriteDatabaseVersion(*m_db, m_ini(SET_DATABASE_VERSION_STATEMENT));

		Parse();
		if (const auto failsCount =
		        TRY("store",
		            [&] {
						return Store(*m_db, m_data);
					});
		    failsCount != 0)
		{
			PLOGE << "Something went wrong";
		}

		TRY("update database", [&] {
			return ExecuteScript(*m_db, "update database", m_ini(DB_UPDATE_SCRIPT, DEFAULT_DB_UPDATE_SCRIPT));
		});

		TRY("CollectCompilations", [this] {
			CollectCompilations();
			return 0;
		});

		TRY("CollectAnnotations", [this] {
			CollectAnnotations();
			return 0;
		});

		TRY("analyze", [&] {
			return Analyze(*m_db);
		});

		ok = true;
	}

	void SetUnknownGenreId()
	{
		if (const auto it = m_genresIndex.find(UNKNOWN_ROOT))
			m_unknownGenreId = *it;
		else
			assert(false && "unexpected UNKNOWN_ROOT");
	}

	auto GetNewInpxFolders()
	{
		InpxFolders diff;
		const auto  inpxFolders = GetInpxFolder(m_ini, true);

		std::unordered_map<QString, std::vector<QString>> result;

		const auto proj = [](const auto& item) {
			return item.first;
		};
		std::ranges::set_difference(inpxFolders, m_data.inpxFolders, std::inserter(diff, diff.end()), {}, proj, proj);

		auto              inpxCaches = inpxFolders | std::views::values;
		std::set<QString> inpxCachesSorted { std::cbegin(inpxCaches), std::cend(inpxCaches) };

		auto              dbCaches = m_data.inpxFolders | std::views::values;
		std::set<QString> dbCachesSorted { std::cbegin(dbCaches), std::cend(dbCaches) };

		if (!std::ranges::includes(inpxCachesSorted, dbCachesSorted))
			m_oldDataUpdateFound = true;

		for (const auto& [folder, file] : diff | std::views::keys)
			result[folder].emplace_back(file);

		return result;
	}

	std::pair<Data, Dictionary> ReadData(const QString& genresFileName)
	{
		Data data;

		const auto dictionaryInserter = [](const DB::IQuery& query) {
			return std::make_pair(query.Get<QString>(1), query.Get<size_t>(0));
		};

		SetNextId(*m_db);
		data.authors               = ReadDictionary<Dictionary>("authors", *m_db, "select AuthorID, LastName||','||FirstName||','||MiddleName from Authors", dictionaryInserter);
		data.series                = ReadDictionary<Dictionary>("series", *m_db, "select SeriesID, SeriesTitle from Series", dictionaryInserter);
		data.keywords              = ReadDictionary<Dictionary>("keywords", *m_db, "select KeywordID, KeywordTitle from Keywords", dictionaryInserter);
		data.bookFolders           = ReadDictionary<Dictionary>("folders", *m_db, "select FolderID, FolderTitle from Folders", dictionaryInserter);
		auto [genres, genresIndex] = ReadGenres(*m_db, genresFileName);
		data.genres                = std::move(genres);
		data.updates               = ReadUpdates(*m_db);
		data.reviews               = ReadReviews(*m_db);

		{
			const auto query = m_db->CreateQuery("select BookID, FolderID, FileName||Ext from Books");
			for (query->Execute(); !query->Eof(); query->Next())
			{
				const auto  bookId   = query->Get<long long>(0);
				const auto  folderId = query->Get<long long>(1);
				const auto* value    = query->Get<const char*>(2);
				m_uniqueFiles.try_emplace(std::make_pair(static_cast<size_t>(folderId), value), bookId);
			}
		}

		const auto inpxFolderInserter = [](const DB::IQuery& query) {
			return std::make_pair(std::make_pair(query.Get<QString>(0), query.Get<QString>(1)), query.Get<QString>(2));
		};
		data.inpxFolders = ReadDictionary<InpxFolders>("inpx", *m_db, "select Folder, File, Hash from Inpx", inpxFolderInserter);

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
			std::vector<QString> folders;
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
			const auto it = newInpxFolders.find(QFileInfo(inpxPath).fileName());
			if (it == newInpxFolders.end())
				continue;

			const auto zip = TRY(QString("open %1").arg(inpxPath), [&] {
				return std::make_unique<Zip>(inpxPath);
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
		auto       folders    = TRY(std::format("iterate {}", inpxFolder), [&] {
			std::vector<QString> result;
			for (QDirIterator it(inpxFolder, { "*" }, QDir::Files); it.hasNext();)
			{
				const auto fileInfo = it.nextFileInfo();
				auto       fileName = fileInfo.fileName();
				if (!(IsOneOf(fileName, COMPILATIONS, CONTENTS) || m_data.bookFolders.contains(fileName)) && Zip::IsArchive(fileInfo.filePath()))
					result.emplace_back(std::move(fileName));
			}
			return result;
		});

		const auto result = folders.size();

		std::ranges::for_each(std::move(folders), [this](auto&& item) {
			m_foldersToParse.push(std::forward<QString>(item));
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
		const auto& dbFileName       = m_ini(DB_PATH);
		const auto  connectionString = QString("path=%1").arg(dbFileName).toStdString();

		m_db = Create(DB::Factory::Impl::Sqlite, connectionString);

		const auto [oldData, oldGenresIndex] = ReadData(m_ini(GENRES, DEFAULT_GENRES));

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
			for (const auto& [key, _] : src)
				dst.erase(key);
		};

		filter(m_data.authors, oldData.authors);
		filter(m_data.series, oldData.series);
		filter(m_data.keywords, oldData.keywords);
		filter(m_data.bookFolders, oldData.bookFolders);
		filter(m_data.inpxFolders, oldData.inpxFolders);
		Filter(m_data.updates);
		Filter(m_data.reviews, oldData.reviews);

		if (const auto failsCount = Store(*m_db, m_data); failsCount != 0)
		{
			PLOGE << "Something went wrong";
		}

		TRY("CollectCompilations", [this] {
			CollectCompilations();
			return 0;
		});

		TRY("CollectAnnotations", [this] {
			CollectAnnotations();
			return 0;
		});

		TRY("analyze", [&] {
			return Analyze(*m_db);
		});

		return result;
	}

	void Parse()
	{
		Timer t("parsing archives");

		auto [genresData, genresIndex] = LoadGenres(m_ini(GENRES, DEFAULT_GENRES));
		m_data.genres                  = std::move(genresData);
		m_genresIndex                  = std::move(genresIndex);
		SetUnknownGenreId();

		for (const auto& inpxPath : GetInpxFilesInFolder(m_ini))
		{
			PLOGI << QString("parsing %1 started").arg(inpxPath);
			const auto inpxContent = ExtractInpxFileNames(inpxPath);

			const auto zip = [&] {
				if (!QFile::exists(inpxPath))
					return std::unique_ptr<Zip> {};

				return TRY(QString("open %1").arg(inpxPath), [&] {
					return std::make_unique<Zip>(inpxPath);
				});
			}();

			if (zip)
				ParseInpxFiles(inpxPath, zip.get(), inpxContent.inpx);

			PLOGI << QString("parsing %1 finished").arg(inpxPath);
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
		PLOGI << "Collect reviews";

		for (QDirIterator it(m_ini(ADDITIONAL_FOLDER) + "/" + REVIEWS_FOLDER, { "*.7z" }, QDir::Files); it.hasNext();)
		{
			const auto fileInfo = it.nextFileInfo();
			auto       fileName = fileInfo.fileName();
			const auto zip      = TRY(QString("open %1").arg(fileName), [&] {
				return std::make_unique<Zip>(fileInfo.filePath());
			});
			if (!zip)
				continue;

			for (const auto& file : zip->GetFileNameList())
				m_data.reviews[file].emplace(fileName);
		}
	}

	void CollectCompilations() const
	{
		const auto fileName = m_ini(ADDITIONAL_FOLDER) + "/" + COMPILATIONS;
		if (!QFile::exists(fileName))
			return;

		const auto zip = TRY(QString("open %1").arg(fileName), [&] {
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

		PLOGI << "collect compilations info";

		{
			const auto query = m_db->CreateQuery(R"(select b.BookID, b.Title
from Books b
join Folders f on f.FolderID = b.FolderID and f.FolderTitle=?
where b.FileName = ? and b.Ext = ?)");

			const auto getBookInfo = [&](const QJsonObject& obj) {
				const auto      folder = obj["folder"].toString();
				const auto      file   = obj["file"].toString();
				const QFileInfo fileInfo(file);
				const auto      name = fileInfo.completeBaseName(), ext = "." + fileInfo.suffix();

				query->Bind(0, folder);
				query->Bind(1, name);
				query->Bind(2, ext);

				query->Execute();

				if (query->Eof())
					return std::make_pair(0LL, QString {});

				const auto bookId = query->Get<long long>(0);
				auto       title  = query->Get<QString>(1).toLower();
				query->Reset();

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
		}

		PLOGI << "write compilations info";
		const auto tr = m_db->CreateTransaction();

		tr->CreateCommand("delete from Compilations_Search")->Execute();
		tr->CreateCommand("delete from Compilations")->Execute();

		{
			const auto command = tr->CreateCommand("insert into Compilations(CompilationID, BookId, Title, Covered) values(?, ?, ?, ?)");
			for (const auto& compilation : compilations)
			{
				assert(!compilation.title.empty());
				auto title = compilation.title.begin()->toStdString();
				for (const auto& token : compilation.title | std::views::drop(1))
					title.append(" ").append(token.toStdString());

				command->Bind(0, compilation.id);
				command->Bind(1, compilation.bookId);
				command->Bind(2, title);
				command->Bind(3, compilation.covered ? 1 : 0);
				command->Execute();
			}
		}
		{
			const auto command = tr->CreateCommand("insert into Compilation_List(CompilationID, BookId, Part) values(?, ?, ?)");
			for (const auto& compilation : compilations)
			{
				for (const auto& book : compilation.books)
				{
					command->Bind(0, compilation.id);
					command->Bind(1, book.first);
					command->Bind(2, book.second);
					command->Execute();
				}
			}
		}

		tr->CreateCommand("INSERT INTO Compilations_Search(Compilations_Search) VALUES('rebuild')")->Execute();

		tr->Commit();
	}

	void CollectAnnotations() const
	{
		if (!(m_mode & CreateCollectionMode::LoadAnnotations))
			return;

		const auto fileName = m_ini(ADDITIONAL_FOLDER) + "/" + ANNOTATIONS;
		if (!QFile::exists(fileName))
			return;

		const auto zip = TRY(QString("open %1").arg(fileName), [&] {
			return std::make_unique<Zip>(fileName);
		});
		if (!zip)
			return;

		const auto tr      = m_db->CreateTransaction();
		const auto command = tr->CreateCommand("insert or ignore into Annotations(BookID, Text) values(?, ?)");

		std::unordered_map<size_t, QStringView> folders;
		for (const auto& [key, value] : m_data.bookFolders)
			folders.try_emplace(value, key);

		const auto books = m_data.books | std::views::transform([&](const Book& item) {
							   const auto it = folders.find(item.folderId);
							   assert(it != folders.end());
							   return std::pair(QString("%1/%2").arg(it->second, item.fileName), static_cast<long long>(item.id));
						   })
		                 | std::ranges::to<std::unordered_map>();

		{
			Timer t("collect annotations");
			for (const auto& file : zip->GetFileNameList() | std::views::filter([this](const QString& item) {
										return m_data.bookFolders.contains(item);
									}))
			{
				const auto        stream = zip->Read(file);
				AnnotationsParser parser(stream->GetStream(), *command, books);
				parser.Parse();
			}
		}

		{
			Timer t("rebuild Annotations_Search");
			tr->CreateCommand("INSERT INTO Annotations_Search(Annotations_Search) VALUES('rebuild')")->Execute();
		}

		Timer t("commit Annotations");
		tr->Commit();
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

			QFileInfo  archiveFileInfo(m_ini(ARCHIVE_FOLDER) + "/" + folder);
			const auto archiveFileName = archiveFileInfo.filePath();
			const auto zip             = TRY(QString("open %1").arg(archiveFileName), [&]() -> std::unique_ptr<Zip> {
				return std::make_unique<Zip>(archiveFileName);
			});
			if (!zip)
				continue;

			Work(folder, *zip, files | std::views::keys | std::ranges::to<std::vector>(), archiveFileInfo.birthTime());
		}
	}

	void ScanUnIndexedFolders()
	{
		if (!(m_mode & CreateCollectionMode::ScanUnIndexedFolders))
			return;

		const auto                   inpxFolder = m_ini(ARCHIVE_FOLDER);
		static constexpr const char* specials[] { "authors", "covers", "images", "reviews", "etc", "faq", "program" };
		const auto                   getUnIndexedFolders = [&] {
			std::vector<QString> result;
			for (QDirIterator it(inpxFolder, { "*" }, QDir::Files, QDirIterator::Subdirectories); it.hasNext();)
			{
				const auto fileInfo = it.nextFileInfo();
				auto       fileName = fileInfo.filePath().mid(inpxFolder.length() + 1);
				if (!(m_foldersContent.contains(fileName) || fileName.endsWith(".inpx") || std::ranges::any_of(specials, [&](const auto* item) {
						  return fileName.startsWith(item);
					  })))
					result.emplace_back(std::move(fileName));
			}

			return result;
		};
		auto folders = TRY(std::format("iterate {}", inpxFolder), getUnIndexedFolders);
		std::ranges::for_each(std::move(folders), [this](auto&& item) {
			m_foldersToParse.push(std::forward<QString>(item));
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

	void ParseInpxFiles(const QString& inpxFileName, const Zip* zipInpx, const std::vector<QString>& inpxFiles)
	{
		if (!zipInpx)
			return;

		[[maybe_unused]] const auto* r = std::setlocale(LC_ALL, "en_US.utf8"); // NOLINT(concurrency-mt-unsafe)

		size_t n = 0;

		GetFieldList(zipInpx);
		for (const auto& fileName : inpxFiles)
			GetDecodedStream(*zipInpx, fileName, [&](QIODevice& zipDecodedStream) {
				n += ProcessInpx(inpxFileName, zipDecodedStream, m_ini(ARCHIVE_FOLDER), fileName);
			});

		PLOGI << n << " rows parsed";
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
				PLOGW << "unexpected field name " << str;
				return &GetBookBufFieldDefault;
			}

			return it->second;
		});
	}

	size_t ProcessInpx(const QString& inpxFileName, QIODevice& stream, const QString& rootFolder, QString folder)
	{
		auto&       checkSum = m_data.inpxFolders[std::make_pair(QFileInfo(inpxFileName).fileName(), folder)];
		QFileInfo   folderInfo(folder);
		const auto  mask          = folderInfo.completeBaseName() + ".*";
		QStringList suitableFiles = QDir(rootFolder).entryList({ mask });
		std::ranges::transform(suitableFiles, suitableFiles.begin(), [](const auto& file) {
			return file.toLower();
		});

		folder = suitableFiles.isEmpty() ? folderInfo.completeBaseName() + "." + m_ini(DEFAULT_ARCHIVE_TYPE) : suitableFiles.front();

		QCryptographicHash hash(QCryptographicHash::Md5);
		size_t             n = 0;
		for (auto byteArray = stream.readLine(); !byteArray.isEmpty(); byteArray = stream.readLine(), ++n)
		{
			ProcessInpxString(rootFolder, folder, byteArray);
			hash.addData(byteArray);
		}

		checkSum = QString::fromUtf8(hash.result().toHex()).toUpper();

		return n;
	}

	auto& GetFileList(const QString& rootFolder, const QStringView folder)
	{
		if (const auto it = m_foldersContent.find(folder); it != m_foldersContent.end())
			return it->second;

		const auto it = m_foldersContent.try_emplace(folder.toString(), std::unordered_map<QString, std::pair<size_t, int>, WStringHash, WStringEqualTo> {}).first;

		auto& fileList = it->second;

		const QFileInfo archiveFileInfo(rootFolder + "/" + it->first);
		if (!archiveFileInfo.exists())
			return fileList;

		Zip archiveFile(archiveFileInfo.filePath());
		std::ranges::transform(archiveFile.GetFileNameList(), std::inserter(fileList, fileList.end()), [&](const auto& item) {
			return std::make_pair(item, std::make_pair(archiveFile.GetFileSize(item), 0));
		});

		return fileList;
	}

	void ProcessInpxString(const QString& rootFolder, const QString& folder, const QByteArray& byteArray)
	{
		const auto& line = m_rawData.emplace_back(QString::fromUtf8(byteArray));
		auto        buf  = ParseBook(line, m_bookBufMapping, folder);

		auto&      fileList = GetFileList(rootFolder, buf.FOLDER);
		const auto it       = fileList.find(buf.fileName);
		const auto found    = it != fileList.end();

		if (!found && !!(m_mode & CreateCollectionMode::SkipLostBooks))
		{
			PLOGW << "'" << buf.TITLE.toUtf8() << "'" << " skipped because its file " << buf.FOLDER.toUtf8() << "/" << buf.fileName << " not found.";
			return;
		}

		AddBook(buf);
		if (found)
			++it->second.second;
	}

	size_t ParseFile(const QString& folder, const Zip& zip, const QString& fileName, const QDateTime& zipDateTime, const QString& originBaseName = {}, const QString& originSuffix = {})
	{
		auto  parseResult = Fb2InpxParser::Parse(folder, zip, fileName, zipDateTime, !!(m_mode & CreateCollectionMode::MarkUnIndexedFilesAsDeleted));
		auto& line        = m_rawData.emplace_back(std::move(parseResult.line));
		if (!originBaseName.isEmpty())
		{
			const QFileInfo fileInfo(fileName);
			line.replace(QString("%1%2%1").arg(QChar { Fb2InpxParser::FIELDS_SEPARATOR }, fileInfo.completeBaseName()), QString("%1%2%1").arg(QChar { Fb2InpxParser::FIELDS_SEPARATOR }, originBaseName));
			if (!originSuffix.isEmpty())
				line.replace(QString("%1%2%1").arg(QChar { Fb2InpxParser::FIELDS_SEPARATOR }, fileInfo.suffix()), QString("%1%2%1").arg(QChar { Fb2InpxParser::FIELDS_SEPARATOR }, originSuffix));
		}
		if (line.isEmpty())
			return INVALID_INDEX;

		auto buf        = ParseBook(line, m_bookBufMapping, folder);
		auto annotation = !!(m_mode & CreateCollectionMode::LoadAnnotations) ? AnnotationsParser::Prepare(std::move(parseResult.annotation)) : QString {};

		std::lock_guard lock(m_dataGuard);
		const auto      index = AddBook(buf);

		if (index != INVALID_INDEX && !annotation.isEmpty())
			m_data.annotations.emplace_back(m_data.books[index].id, std::move(annotation));

		return index;
	}

	size_t ParseDate(const QStringView date, Data& data)
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
		const auto year    = QNext(itDate, endDate, DATE_SEPARATOR).toInt();
		const auto month   = QNext(itDate, endDate, DATE_SEPARATOR).toInt();

		const auto itYear      = data.updates.children.find(year);
		auto&      yearUpdate  = itYear != data.updates.children.end() ? itYear->second : data.updates.children.try_emplace(year, createUpdate(year, 0)).first->second;
		const auto itMonth     = yearUpdate.children.find(month);
		auto&      monthUpdate = itMonth != yearUpdate.children.end() ? itMonth->second : yearUpdate.children.try_emplace(month, createUpdate(month, yearUpdate.id)).first->second;
		return monthUpdate.id;
	}

	size_t AddBook(Book& buf)
	{
		const auto idFolder = Add<QStringView, long long, -1>(buf.FOLDER, m_data.bookFolders);
		const auto seriesId = Add<QStringView, int, -1>(buf.SERIES, m_data.series);

		{
			const auto [it, inserted] = m_uniqueFiles.try_emplace(std::make_pair(idFolder, buf.fileName), 0);
			if (inserted)
				buf.id = it->second = GetId();

			const auto serNo = ToInteger<int>(buf.SERNO, -1);
			if (seriesId != -1)
				m_data.booksSeries[it->second].emplace_back(seriesId, IsOneOf(serNo, 0, -1) ? std::nullopt : std::optional(serNo));

			if (!inserted)
				return INVALID_INDEX;
		}

		buf.folderId = idFolder;

		auto authorIds = ParseItem(buf.AUTHOR, m_data.authors, Fb2InpxParser::LIST_SEPARATOR, &ParseCheckerAuthor);
		if (authorIds.empty())
			authorIds = ParseItem(AUTHOR_UNKNOWN_STR, m_data.authors);
		assert(!authorIds.empty() && "a book cannot be an orphan");
		std::ranges::copy(authorIds, std::back_inserter(m_data.booksAuthors[buf.id]));

		auto idGenres = ParseItem(
			buf.GENRE,
			m_genresIndex,
			Fb2InpxParser::LIST_SEPARATOR,
			&ParseCheckerDefault,
			[&, &data = m_data.genres](const QStringView newItemTitle) {
				const auto title        = newItemTitle.toString();
				const auto result       = std::size(data);
				auto&      genre        = data.emplace_back(title, "", title, m_unknownGenreId);
				auto&      unknownGenre = data[m_unknownGenreId];
				genre.dbCode            = QString("%1.%2").arg(unknownGenre.dbCode).arg(++unknownGenre.childrenCount);
				m_unknownGenres.push_back(genre.name);
				return result;
			},
			[&data = m_data.genres](const Dictionary& container, const QStringView value) {
				if (auto itGenre = container.find(value))
					return itGenre;

				return container.find_if([value, &data](const auto& item) {
					return data[item.second].name.compare(value, Qt::CaseInsensitive) == 0;
				});
			}
		);

		buf.updateId = ParseDate(buf.DATE, m_data);

		std::ranges::copy(idGenres, std::back_inserter(m_data.booksGenres[buf.id]));

		if (!buf.KEYWORDS.empty())
			if (const auto keywordIds = ParseKeywords(buf.KEYWORDS, m_data.keywords); !keywordIds.empty())
				std::ranges::copy(keywordIds, std::back_inserter(m_data.booksKeywords[buf.id]));

		buf.LANG = GetLanguage(buf.LANG);

		m_data.books.emplace_back(std::move(buf));
		PLOGI_IF((m_data.books.size() % LOG_INTERVAL) == 0) << m_data.books.size() << " books added";

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
	const Ini                      m_ini;
	const CreateCollectionMode     m_mode;
	const Callback                 m_callback;
	std::shared_ptr<DB::IDatabase> m_db;
	std::unique_ptr<IExecutor>     m_executor;

	std::list<QString>                                                                                                                         m_rawData;
	Data                                                                                                                                       m_data;
	Dictionary                                                                                                                                 m_genresIndex;
	std::atomic_uint64_t                                                                                                                       m_parsedN { 0 };
	std::vector<QString>                                                                                                                       m_unknownGenres;
	size_t                                                                                                                                     m_unknownGenreId { 0 };
	std::unordered_set<size_t>                                                                                                                 m_newUpdates;
	std::unordered_map<QString, QString>                                                                                                       m_uniqueKeywords;
	std::unordered_map<std::pair<size_t, QString>, size_t, PairHash<size_t, QString>>                                                          m_uniqueFiles;
	std::unordered_map<QString, std::unordered_map<QString, std::pair<size_t, int>, WStringHash, WStringEqualTo>, WStringHash, WStringEqualTo> m_foldersContent;
	bool                                                                                                                                       m_oldDataUpdateFound { false };

	BookBufMapping m_bookBufMapping;

	std::queue<QString>     m_foldersToParse;
	std::mutex              m_foldersToParseGuard;
	std::condition_variable m_foldersToParseCondition;

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
		const Ini  ini(data);
		const auto folders = GetInpxFolder(ini, true);

		const auto command = transaction.CreateCommand("INSERT INTO Inpx (Folder, File, Hash) VALUES (?, ?, ?)");
		for (const auto& [first, second] : folders)
		{
			command->Bind(0, first.first);
			command->Bind(1, first.second);
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
		const Ini  ini(data);
		const auto inpxFolders = GetInpxFolder(ini, false);

		InpxFolders dbFolders;
		const auto  query = database.CreateQuery("select Folder, File, Hash from Inpx");
		for (query->Execute(); !query->Eof(); query->Next())
			dbFolders.try_emplace(std::make_pair(query->Get<const char*>(0), query->Get<const char*>(1)), query->Get<const char*>(2));

		InpxFolders diff;
		const auto  proj = [](const auto& item) {
			return item.first;
		};
		std::ranges::set_difference(inpxFolders, dbFolders, std::inserter(diff, diff.end()), {}, proj, proj);
		return !diff.empty();
	};

	return TRY("fill inpx", process);
}
