#include "Requester.h"

#include <ranges>

#include <QBuffer>
#include <QByteArray>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#include <unicode/translit.h>

#include "fnd/FindPair.h"
#include "fnd/IsOneOf.h"
#include "fnd/ScopedCall.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/Enums.h"
#include "interface/constants/GenresLocalization.h"
#include "interface/constants/Localization.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseController.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IScriptController.h"

#include "logic/data/DataItem.h"
#include "logic/data/Genre.h"
#include "logic/shared/ImageRestore.h"
#include "util/Fb2InpxParser.h"
#include "util/FunctorExecutionForwarder.h"
#include "util/SortString.h"
#include "util/localization.h"
#include "util/timer.h"
#include "util/xml/XmlWriter.h"

#include "log.h"
#include "root.h"
#include "zip.h"

namespace HomeCompa::Opds
{
#define OPDS_REQUEST_ROOT_ITEM(NAME) QByteArray PostProcess_##NAME(const IPostProcessCallback& callback, QIODevice& stream, ContentType contentType, const QStringList&);
OPDS_REQUEST_ROOT_ITEMS_X_MACRO
#undef OPDS_REQUEST_ROOT_ITEM

#define OPDS_REQUEST_ROOT_ITEM(NAME) std::unique_ptr<Flibrary::IAnnotationController::IStrategy> CreateAnnotationControllerStrategy_##NAME(const ISettings&);
OPDS_REQUEST_ROOT_ITEMS_X_MACRO
#undef OPDS_REQUEST_ROOT_ITEM

}

using namespace HomeCompa;
using namespace Opds;

namespace
{

constexpr std::pair<const char*, QByteArray (*)(const IPostProcessCallback&, QIODevice&, ContentType, const QStringList&)> POSTPROCESSORS[] {
#define OPDS_REQUEST_ROOT_ITEM(NAME) { "/" #NAME, &PostProcess_##NAME },
	OPDS_REQUEST_ROOT_ITEMS_X_MACRO
#undef OPDS_REQUEST_ROOT_ITEM
};

constexpr std::pair<const char*, std::unique_ptr<Flibrary::IAnnotationController::IStrategy> (*)(const ISettings&)> ANNOTATION_CONTROLLER_STRATEGY_CREATORS[] {
#define OPDS_REQUEST_ROOT_ITEM(NAME) { "/" #NAME, &CreateAnnotationControllerStrategy_##NAME },
	OPDS_REQUEST_ROOT_ITEMS_X_MACRO
#undef OPDS_REQUEST_ROOT_ITEM
};

constexpr auto SELECT_BOOKS_STARTS_WITH = "select substr(b.SearchTitle, %2, 1), count(42) "
										  "from Books b "
										  "%1 "
										  "left join Books_User bu on bu.BookID = b.BookID "
										  "where b.SearchTitle != ? and b.SearchTitle like ? %3 "
										  "group by substr(b.SearchTitle, %2, 1)";

constexpr auto SELECT_BOOKS = R"(
select b.BookID, b.Title || b.Ext, 0,
	(select a.LastName || coalesce(' ' || nullif(substr(a.FirstName, 1, 1), '') || '.' || coalesce(nullif(substr(a.middleName, 1, 1), '') || '.', ''), '')
		from Authors a
		join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID ORDER BY a.ROWID ASC LIMIT 1
	), s.SeriesTitle, b.SeqNumber
	from Books b
	%1
	left join Series s on s.SeriesID = b.SeriesID
	left join Books_User bu on bu.BookID = b.BookID
	%2
)";
constexpr auto SELECT_BOOKS_WHERE = "where b.SearchTitle %1 ?";

constexpr auto SELECT_AUTHORS_STARTS_WITH = "select substr(a.SearchName, %2, 1), count(42) "
											"from Authors a "
											"join Author_List b on b.AuthorID = a.AuthorID "
											"%1 "
											"where a.SearchName != ? and a.SearchName like ? "
											"group by substr(a.SearchName, %2, 1)";

constexpr auto SELECT_AUTHORS = "select a.AuthorID, a.LastName || coalesce(' ' || nullif(substr(a.FirstName, 1, 1), '') || '.' || coalesce(nullif(substr(a.middleName, 1, 1), '') || '.', ''), ''), count(42) "
								"from Authors a "
								"join Author_List b on b.AuthorID = a.AuthorID "
								"%1 "
								"where a.SearchName %2 ? "
								"group by a.AuthorID";

const auto SELECT_BOOK_COUNT = "select count(42) from Books b %1 left join Books_User bu on bu.BookID = b.BookID %2";

constexpr auto JOIN_ARCHIVE = "join Books gl on gl.BookID = b.BookID and gl.FolderID = %1";
constexpr auto JOIN_AUTHOR = "join Author_List al on al.BookID = b.BookID and al.AuthorID = %1";
constexpr auto JOIN_GENRE = "join Genre_List gl on gl.BookID = b.BookID and gl.GenreCode = '%1'";
constexpr auto JOIN_GROUP = "join Groups_List_User gl on gl.BookID = b.BookID and gl.GroupID = %1";
constexpr auto JOIN_KEYWORD = "join Keyword_List gl on gl.BookID = b.BookID and gl.KeywordID = %1";
constexpr auto JOIN_SERIES = "join Series_List gl on gl.BookID = b.BookID and gl.SeriesID = %1";
constexpr auto JOIN_SEARCH = "join Ids i on i.BookID = b.BookID";

constexpr auto WITH_SEARCH = R"(
with Ids(BookID) as (
    with Search (Title) as (
        select ?
    )
    select b.BookID
        from Books b
        join Books_Search fts on fts.rowid = b.BookID
        join Search s on Books_Search match s.Title
    union
    select b.BookID
        from Books b
        join Author_List l on l.BookID = b.BookID
        join Authors_Search fts on fts.rowid = l.AuthorID
        join Search s on Authors_Search match s.Title
    union
    select b.BookID
        from Books b
        join Series_List l on l.BookID = b.BookID
        join Series_Search fts on fts.rowid = l.SeriesID
        join Search s on Series_Search match s.Title
)
)";

constexpr auto BOOKS_NOT_DELETED = "coalesce(bu.IsDeleted, b.IsDeleted, 0) = 0";

constexpr auto CONTEXT = "Requester";
constexpr auto COUNT = QT_TRANSLATE_NOOP("Requester", "Number of: %1");
constexpr auto BOOK = QT_TRANSLATE_NOOP("Requester", "Book");
constexpr auto BOOKS = QT_TRANSLATE_NOOP("Requester", "Books");
constexpr auto SEARCH_RESULTS = QT_TRANSLATE_NOOP("Requester", R"(Books found for the request "%1": %2)");
constexpr auto NOTHING_FOUND = QT_TRANSLATE_NOOP("Requester", R"(No books found for the request "%1")");
constexpr auto PREVIOUS = QT_TRANSLATE_NOOP("Requester", "[Previous page]");
constexpr auto NEXT = QT_TRANSLATE_NOOP("Requester", "[Next page]");
constexpr auto FIRST = QT_TRANSLATE_NOOP("Requester", "[First page]");
constexpr auto LAST = QT_TRANSLATE_NOOP("Requester", "[Last page]");

constexpr auto ENTRY = "entry";
constexpr auto TITLE = "title";
constexpr auto CONTENT = "content";
constexpr auto OPDS_TRANSLITERATE = "opds/transliterate";
constexpr auto OPDS_BOOK_LIMIT_KEY = "opds/BookEntryLimit";
constexpr auto OPDS_BOOK_LIMIT_DEFAULT = 25;

TR_DEF

constexpr std::pair<const char*, std::tuple<const char*, const char*>> HEAD_QUERIES[] = {
	{  Loc::Authors, { "select LastName || ' ' || FirstName || ' ' || middleName from Authors where AuthorID = ?", nullptr } },
	{   Loc::Genres,								   { "select FB2Code from Genres where GenreCode = ?", Flibrary::GENRE } },
	{   Loc::Series,										{ "select SeriesTitle from Series where SeriesID = ?", nullptr } },
	{ Loc::Keywords,									{ "select KeywordTitle from Keywords where KeywordID = ?", nullptr } },
	{ Loc::Archives,									   { "select FolderTitle from Folders where FolderID = ?", nullptr } },
	{   Loc::Groups,										  { "select Title from Groups_User where GroupID = ?", nullptr } },
};

struct Node
{
	using Attributes = std::vector<std::pair<QString, QString>>;
	using Children = std::vector<Node>;
	QString name;
	QString value;
	Attributes attributes;
	Children children;

	QString title;

	QString author;
	QString series;
	int seqNum;
};

const QString& GetContent(const Node& node)
{
	const auto it = std::ranges::find(node.children, CONTENT, [](const auto& item) { return item.name; });
	assert(it != node.children.end());
	return it->value;
}

std::tuple<Util::QStringWrapper, Util::QStringWrapper, int, Util::QStringWrapper> ToComparable(const Node& node)
{
	return node.author.isEmpty() ? std::make_tuple(Util::QStringWrapper { GetContent(node) }, Util::QStringWrapper { node.title }, 0, Util::QStringWrapper { node.author })
	                             : std::make_tuple(Util::QStringWrapper { node.author }, Util::QStringWrapper { node.series }, node.seqNum, Util::QStringWrapper { node.title });
}

bool operator<(const Node& lhs, const Node& rhs)
{
	return ToComparable(lhs) < ToComparable(rhs);
}

std::vector<Node> GetStandardNodes(QString id, QString title)
{
	return std::vector<Node> {
		{ "updated", QDateTime::currentDateTime().toUTC().toString("yyyy-MM-ddThh:mm:ssZ") },
		{      "id",														 std::move(id) },
		{     TITLE,													  std::move(title) },
	};
}

Util::XmlWriter& operator<<(Util::XmlWriter& writer, const Node& node)
{
	const auto nodeGuard = writer.Guard(node.name);
	std::ranges::for_each(node.attributes, [&](const auto& item) { writer.WriteAttribute(item.first, item.second); });
	writer.WriteCharacters(node.value);
	std::ranges::for_each(node.children, [&](const auto& item) { writer << item; });
	return writer;
}

void PrepareQueryForLike(QString& text, QStringList& arguments)
{
	if (!text.contains("like", Qt::CaseInsensitive))
		return;

	bool ok = true;
	for (auto& argument : arguments)
	{
		if (argument.contains('_'))
		{
			ok = false;
			break;
		}
		if (!argument.isEmpty() && std::ranges::any_of(arguments | std::views::take(arguments.length() - 1), [](const auto ch) { return ch == '%'; }))
		{
			ok = false;
			break;
		}
	}

	if (ok)
		return;

	static constexpr char ESCAPE[] { '\\', '|', '#', '@', '~', '^' };
	const auto it = std::ranges::find_if(ESCAPE, [&](const auto ch) { return std::ranges::none_of(arguments, [&](const auto item) { return item.contains(ch); }); });
	assert(it != std::end(ESCAPE));

	for (auto& argument : arguments)
	{
		const auto endsWithPct = argument.endsWith('%');
		if (endsWithPct)
			argument.removeLast();

		argument.replace('_', QString("%1_").arg(*it));
		argument.replace('%', QString("%1%").arg(*it));
		if (endsWithPct)
			argument.append('%');
	}

	text.replace(QRegularExpression(R"(like +(\?))"), QString(R"(like \1 ESCAPE '%1')").arg(*it));
}

std::unique_ptr<DB::IQuery> CreateQuery(DB::IDatabase& db, QString text, QStringList arguments)
{
	PrepareQueryForLike(text, arguments);
	auto query = db.CreateQuery(text.toStdString());
	for (int n = 0; const auto& argument : arguments)
		query->Bind(n++, argument.toStdString());
	return query;
}

QString ParseTitle(DB::IDatabase& db, const QString& type, const QString& id)
{
	const auto it = std::ranges::find(HEAD_QUERIES, type, [](const auto& item) { return item.first; });
	if (it == std::end(HEAD_QUERIES))
		return {};

	const auto& [queryText, context] = it->second;

	const auto query = CreateQuery(db, queryText, { id });
	query->Execute();
	assert(!query->Eof());
	return context ? Loc::Tr(context, query->Get<const char*>(0)) : query->Get<const char*>(0);
}

QString ParseTitle(DB::IDatabase& db, QString title)
{
	const auto splitted = title.split('/', Qt::SkipEmptyParts);
	return IsOneOf(splitted.size(), 2, 3) ? ParseTitle(db, splitted.front(), splitted.back()) : splitted.size() == 5 ? ParseTitle(db, splitted[1], splitted.back()) : std::move(title);
}

Node GetHead(DB::IDatabase& db, QString id, QString title, QString root, QString self)
{
	title = ParseTitle(db, std::move(title));
	auto standardNodes = GetStandardNodes(std::move(id), std::move(title));
	standardNodes.emplace_back("link",
	                           QString {
    },
	                           Node::Attributes { { "href", root }, { "rel", "start" }, { "type", "application/atom+xml;profile=opds-catalog;kind=navigation" } });
	standardNodes.emplace_back("link",
	                           QString {
    },
	                           Node::Attributes { { "href", std::move(self) }, { "rel", "self" }, { "type", "application/atom+xml;profile=opds-catalog;kind=navigation" } });
	standardNodes.emplace_back("link",
	                           QString {
    },
	                           Node::Attributes { { "href", QString("%1/opensearch").arg(root) }, { "rel", "search" }, { "type", "application/opensearchdescription+xml" } });
	standardNodes.emplace_back("link",
	                           QString {
    },
	                           Node::Attributes { { "href", QString("%1/search?q={searchTerms}").arg(root) }, { "rel", "search" }, { "type", "application/atom+xml" } });
	return Node {
		"feed",
		{},
		{
          { "xmlns", "http://www.w3.org/2005/Atom" },
          { "xmlns:dc", "http://purl.org/dc/terms/" },
          { "xmlns:opds", "http://opds-spec.org/2010/catalog" },
		  },
		std::move(standardNodes)
	};
}

Node& WriteEntry(const QString& root, Node::Children& children, QString id, QString title, const int count, QString content = {}, const bool isCatalog = true)
{
	auto href = QString("%1/%2").arg(root, id);
	auto& entry = children.emplace_back(ENTRY, QString {}, Node::Attributes {}, GetStandardNodes(std::move(id), title));
	entry.title = std::move(title);
	if (isCatalog)
		entry.children.emplace_back("link",
		                            QString {
        },
		                            Node::Attributes { { "href", std::move(href) }, { "rel", "subsection" }, { "type", "application/atom+xml;profile=opds-catalog;kind=navigation" } });
	if (content.isEmpty() && count > 0)
		content = Tr(COUNT).arg(count);
	if (!content.isEmpty())
		entry.children.emplace_back(CONTENT,
		                            std::move(content),
		                            Node::Attributes {
										{ "type", "text/html" }
        });
	return entry;
}

std::vector<std::pair<QString, int>> ReadStartsWith(DB::IDatabase& db, const QString& queryText, const QString& value = {})
{
	Util::Timer t(L"ReadStartsWith: " + value.toStdWString());

	std::vector<std::pair<QString, int>> result;

	const auto query = CreateQuery(db, queryText, { value, value + "%" });
	for (query->Execute(); !query->Eof(); query->Next())
		result.emplace_back(query->Get<const char*>(0), query->Get<int>(1));

	std::ranges::sort(result, [](const auto& lhs, const auto& rhs) { return Util::QStringWrapper { lhs.first } < Util::QStringWrapper { rhs.first }; });
	std::ranges::transform(result, result.begin(), [&](const auto& item) { return std::make_pair(value + item.first, item.second); });

	return result;
}

void WriteNavigationEntries(DB::IDatabase& db, const char* navigationType, const QString& queryText, const QString& arg, const QString& root, Node::Children& children)
{
	Util::Timer t(L"WriteNavigationEntry: " + arg.toStdWString());
	const auto query = CreateQuery(db, queryText, { arg });
	for (query->Execute(); !query->Eof(); query->Next())
	{
		const auto id = query->Get<int>(0);
		auto entryId = QString("%1/%2").arg(navigationType).arg(id);
		auto href = QString("%1/%2").arg(root, entryId);
		auto content = query->ColumnCount() > 3 ? query->Get<const char*>(3) : QString {};

		auto& entry = WriteEntry(root, children, std::move(entryId), query->Get<const char*>(1), query->Get<int>(2), std::move(content));
		entry.children.emplace_back("link",
		                            QString {
        },
		                            Node::Attributes { { "href", std::move(href) }, { "rel", "subsection" }, { "type", "application/atom+xml;profile=opds-catalog;kind=navigation" } });
	}
}

void WriteBookEntries(DB::IDatabase& db, const char*, const QString& queryText, const QString& arg, const QString& root, Node::Children& children)
{
	Util::Timer t(L"WriteBookEntries: " + arg.toStdWString());
	const auto query = CreateQuery(db, queryText, { arg });
	for (query->Execute(); !query->Eof(); query->Next())
	{
		const auto id = query->Get<int>(0);
		auto entryId = QString("Book/%1").arg(id);
		auto href = QString("%1/%2").arg(root, entryId);
		QString author = query->ColumnCount() > 3 ? query->Get<const char*>(3) : QString {};
		QString series = query->ColumnCount() > 4 ? query->Get<const char*>(4) : QString {};
		const int seqNum = query->ColumnCount() > 5 ? query->Get<int>(5) : -1;

		auto content = QString("%1 %2%3").arg(author, series, seqNum > 0 ? QString(" #%1").arg(seqNum) : QString {});

		auto& entry = WriteEntry(root, children, std::move(entryId), query->Get<const char*>(1), query->Get<int>(2), std::move(content), false);
		entry.children.emplace_back("link",
		                            QString {
        },
		                            Node::Attributes { { "href", std::move(href) }, { "rel", "subsection" }, { "type", "application/atom+xml;profile=opds-catalog;kind=acquisition" } });

		entry.author = std::move(author);
		entry.series = std::move(series);
		entry.seqNum = seqNum > 0 ? seqNum : std::numeric_limits<int>::max();
	}
}

using EntryWriter = void (*)(DB::IDatabase& db, const char* navigationType, const QString& queryText, const QString& arg, const QString& root, Node::Children& children);

Node WriteNavigationStartsWith(DB::IDatabase& db,
                               const QString& value,
                               const char* navigationType,
                               const QString& root,
                               const QString& self,
                               const QString& startsWithQuery,
                               const QString& itemQuery,
                               const EntryWriter entryWriter)
{
	auto head = GetHead(db, navigationType, Loc::Tr(Loc::NAVIGATION, navigationType), root, self);
	auto& children = head.children;

	std::vector<std::pair<QString, int>> dictionary {
		{ value, 0 }
	};
	do
	{
		decltype(dictionary) buffer;
		std::ranges::sort(dictionary, [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });
		while (!dictionary.empty() && dictionary.size() + children.size() + buffer.size() < 20)
		{
			auto [ch, count] = std::move(dictionary.back());
			dictionary.pop_back();

			if (!ch.isEmpty())
				entryWriter(db, navigationType, itemQuery.arg("="), ch, root, children);

			auto startsWith = ReadStartsWith(db, startsWithQuery.arg(ch.length() + 1), ch);
			auto tail = std::ranges::partition(startsWith, [](const auto& item) { return item.second > 1; });
			for (const auto& singleCh : tail | std::views::keys)
				entryWriter(db, navigationType, itemQuery.arg("like"), singleCh + "%", root, children);

			startsWith.erase(std::begin(tail), std::end(startsWith));
			std::ranges::copy(startsWith, std::back_inserter(buffer));
		}

		std::ranges::copy(buffer, std::back_inserter(dictionary));
		buffer.clear();
	} while (!dictionary.empty() && dictionary.size() + children.size() < 20);

	for (const auto& [ch, count] : dictionary)
	{
		auto id = QString("%1/starts/%2").arg(navigationType, ch);
		id.replace(' ', "%20");
		auto title = ch;
		if (title.endsWith(' '))
			title[title.length() - 1] = QChar(0xB7);
		WriteEntry(root, children, id, QString("%1~").arg(title), count);
	}

	std::ranges::sort(children, [](const auto& lhs, const auto& rhs) { return Util::QStringWrapper { lhs.title } < Util::QStringWrapper { rhs.title }; });

	return head;
}

QByteArray Decompress(const QString& path, const QString& archive, const QString& fileName, const bool restoreImages)
{
	const Zip unzip(path + "/" + archive);
	const auto stream = unzip.Read(fileName);
	if (!restoreImages)
		return stream->GetStream().readAll();

	QByteArray data;
	{
		QBuffer buffer(&data);
		const ScopedCall bufferGuard([&] { buffer.open(QIODevice::WriteOnly); }, [&] { buffer.close(); });
		buffer.write(Flibrary::RestoreImages(stream->GetStream(), path + "/" + archive, fileName));
	}
	return data;
}

QByteArray Compress(QByteArray data, const QString& fileName)
{
	QByteArray zippedData;
	{
		QBuffer buffer(&zippedData);
		const ScopedCall bufferGuard([&] { buffer.open(QIODevice::WriteOnly); }, [&] { buffer.close(); });
		buffer.open(QIODevice::WriteOnly);
		Zip zip(buffer, Zip::Format::Zip);
		std::vector<std::pair<QString, QByteArray>> toZip {
			{ fileName, std::move(data) }
		};
		zip.Write(std::move(toZip));
	}
	return zippedData;
}

void Transliterate(const char* id, QString& str)
{
	UErrorCode status = U_ZERO_ERROR;
	icu_77::Transliterator* myTrans = icu_77::Transliterator::createInstance(id, UTRANS_FORWARD, status);
	assert(myTrans);

	auto s = str.toStdU32String();
	auto icuString = icu_77::UnicodeString::fromUTF32(reinterpret_cast<int32_t*>(s.data()), static_cast<int32_t>(s.length()));
	myTrans->transliterate(icuString);
	auto buf = std::make_unique_for_overwrite<int32_t[]>(icuString.length() * 4ULL);
	icuString.toUTF32(buf.get(), icuString.length() * 4, status);
	str = QString::fromStdU32String(std::u32string(reinterpret_cast<char32_t*>(buf.get())));
}

QString GetOutputFileNameTemplate(const ISettings& settings)
{
	auto outputFileNameTemplate = settings.Get(Flibrary::Constant::Settings::EXPORT_TEMPLATE_KEY, Flibrary::IScriptController::GetDefaultOutputFileNameTemplate());
	Flibrary::IScriptController::SetMacro(outputFileNameTemplate, Flibrary::IScriptController::Macro::UserDestinationFolder, "");
	return outputFileNameTemplate;
}

QString PrepareSearchTerms(const QString& searchTerms)
{
	auto terms = searchTerms.split(QRegularExpression(R"(\s+|\+)"), Qt::SkipEmptyParts);
	std::ranges::transform(terms, terms.begin(), [](const auto& item) { return item + '*'; });
	return terms.join(' ');
}

template <typename T>
QJsonObject FromQuery(const DB::IQuery& query)
{
	QJsonObject object;
	for (size_t i = 0, sz = query.ColumnCount(); i < sz; ++i)
		object.insert(QString::fromStdString(query.ColumnName(i)), query.Get<T>(i));
	return object;
}

class AnnotationControllerObserver : public Flibrary::IAnnotationController::IObserver
{
	using Functor = std::function<void(const Flibrary::IAnnotationController::IDataProvider& dataProvider)>;

public:
	explicit AnnotationControllerObserver(Functor f)
		: m_f { std::move(f) }
	{
	}

private: // IAnnotationController::IObserver
	void OnAnnotationRequested() override
	{
	}

	void OnAnnotationChanged(const Flibrary::IAnnotationController::IDataProvider& dataProvider) override
	{
		m_f(dataProvider);
	}

	void OnJokeChanged(const QString&) override
	{
	}

	void OnArchiveParserProgress(int /*percents*/) override
	{
	}

private:
	const Functor m_f;
};

} // namespace

class Requester::Impl : public IPostProcessCallback
{
public:
	Impl(std::shared_ptr<const ISettings> settings,
	     std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
	     std::shared_ptr<const Flibrary::IDatabaseController> databaseController,
	     std::shared_ptr<const Flibrary::IAuthorAnnotationController> authorAnnotationController,
	     std::shared_ptr<Flibrary::IAnnotationController> annotationController)
		: m_settings { std::move(settings) }
		, m_collectionProvider { std::move(collectionProvider) }
		, m_databaseController { std::move(databaseController) }
		, m_authorAnnotationController { std::move(authorAnnotationController) }
		, m_annotationController { std::move(annotationController) }
		, m_outputFileNameTemplate { GetOutputFileNameTemplate(*m_settings) }
	{
		m_coversTimer.setInterval(std::chrono::minutes(1));
		m_coversTimer.setSingleShot(true);
		QObject::connect(&m_coversTimer,
		                 &QTimer::timeout,
		                 [this]
		                 {
							 std::lock_guard lock(m_coversGuard);
							 m_covers.clear();
						 });
	}

	const Flibrary::ICollectionProvider& GetCollectionProvider() const
	{
		return *m_collectionProvider;
	}

	Node WriteRoot(const QString& root, const QString& self) const
	{
		const auto db = m_databaseController->GetDatabase(true);
		auto head = GetHead(*db, "root", m_collectionProvider->GetActiveCollection().name, root, self);

		static constexpr std::pair<Flibrary::NavigationMode, const char*> navigationTypes[] {
#define OPDS_ROOT_ITEM(NAME) { Flibrary::NavigationMode::NAME, Loc::NAME },
			OPDS_ROOT_ITEMS_X_MACRO
#undef OPDS_ROOT_ITEM
		};
		QStringList dbStatQueryTextItems;
		std::ranges::transform(navigationTypes | std::views::filter([](const auto& item) { return item.first != Flibrary::NavigationMode::Groups; }),
		                       std::back_inserter(dbStatQueryTextItems),
		                       [this](const auto& item) { return QString("select '%1', count(42) from %1 %2").arg(item.second).arg(ShowRemoved() ? "" : QString(" where IsDeleted = 0")); });

		auto dbStatQueryText = dbStatQueryTextItems.join(" union all ");
		dbStatQueryText.replace("count(42) from Archives", "count(42) from Folders");

		{
			const auto query = db->CreateQuery(dbStatQueryText.toStdString());
			for (query->Execute(); !query->Eof(); query->Next())
			{
				const auto* id = query->Get<const char*>(0);
				if (const auto count = query->Get<int>(1))
					WriteEntry(root, head.children, id, Loc::Tr(Loc::NAVIGATION, id), count);
			}
		}
		{
			const auto query = db->CreateQuery("select g.GroupID, g.Title, count(42) from Groups_User g join Groups_List_User l on l.GroupID = g.GroupID group by g.GroupID");
			for (query->Execute(); !query->Eof(); query->Next())
			{
				const auto id = QString("%1/%2").arg(Loc::Groups).arg(query->Get<int>(0));
				WriteEntry(root, head.children, id, query->Get<const char*>(1), query->Get<int>(2));
			}
		}

		return head;
	}

	Node WriteSearch(const QString& root, const QString& self, const QString& searchTerms, const QString& start) const
	{
		const auto db = m_databaseController->GetDatabase(true);
		auto head = GetHead(*db, "search", Tr(SEARCH_RESULTS).arg(searchTerms), root, self);

		auto terms = searchTerms.split(QRegularExpression(R"(\s+|\+)"), Qt::SkipEmptyParts);
		const auto termsGui = terms.join(' ');
		std::ranges::transform(terms, terms.begin(), [](const auto& item) { return item + '*'; });
		const auto n = head.children.size();
		WriteBookEntries(*db, "", QString("%1%2").arg(WITH_SEARCH, SELECT_BOOKS).arg(JOIN_SEARCH, ShowRemoved() ? "" : QString("where %1").arg(BOOKS_NOT_DELETED)), terms.join(' '), root, head.children);

		{
			const auto it = std::ranges::find(head.children, TITLE, [](const auto& item) { return item.name; });
			assert(it != head.children.end());
			it->value = n == head.children.size() ? Tr(NOTHING_FOUND).arg(termsGui) : Tr(SEARCH_RESULTS).arg(termsGui).arg(head.children.size() - n);
		}
		{
			const auto it = std::ranges::find(head.children, ENTRY, [](const auto& item) { return item.name; });
			const auto startEntryIndex = std::distance(head.children.begin(), it);
			std::sort(it, head.children.end());

			const auto selectionSize = static_cast<ptrdiff_t>(head.children.size());
			const auto maxResultSize = GetMaxResultSize();
			const auto startResultIndex = start.isEmpty() ? 0 : std::clamp(start.toLongLong(), 0LL, selectionSize - startEntryIndex - 1);
			const auto tailSize = selectionSize - (startEntryIndex + startResultIndex + maxResultSize);

			if (tailSize > 0)
				head.children.erase(std::next(head.children.begin(), selectionSize - tailSize), head.children.end());
			head.children.erase(std::next(head.children.begin(), startEntryIndex), std::next(head.children.begin(), startEntryIndex + startResultIndex));

			const auto writeNextPage = [&](QString title, const ptrdiff_t nextPageIndex, const ptrdiff_t pos)
			{
				const QUrl url(self + QString("&start=%1").arg(nextPageIndex));
				auto& entry = *head.children.emplace(std::next(head.children.begin(), pos), ENTRY, QString {}, Node::Attributes {}, GetStandardNodes(QString::number(nextPageIndex), title));
				entry.title = std::move(title);
				entry.children.emplace_back(
					"link",
					QString {
                },
					Node::Attributes { { "href", url.toString(QUrl::FullyEncoded) }, { "rel", "subsection" }, { "type", "application/atom+xml;profile=opds-catalog;kind=navigation" } });
			};

			if (startResultIndex > 0)
			{
				writeNextPage(Tr(FIRST), 0LL, startEntryIndex);
				if (startResultIndex - maxResultSize > 0)
					writeNextPage(Tr(PREVIOUS), std::max(startResultIndex - maxResultSize, 0LL), startEntryIndex + 1);
			}

			if (tailSize > 0)
			{
				auto lastPageIndex = (selectionSize - startEntryIndex) / maxResultSize * maxResultSize;
				if (lastPageIndex == selectionSize - startEntryIndex)
					lastPageIndex -= maxResultSize;

				writeNextPage(Tr(LAST), lastPageIndex, static_cast<ptrdiff_t>(head.children.size()));
				if (tailSize > maxResultSize)
					writeNextPage(Tr(NEXT), startResultIndex + maxResultSize, static_cast<ptrdiff_t>(head.children.size()) - 1);
			}
		}

		return head;
	}

	Node WriteBook(const QString& root, const QString& self, const QString& bookId) const
	{
		Node head;

		QEventLoop eventLoop;

		AnnotationControllerObserver observer(
			[&](const Flibrary::IAnnotationController::IDataProvider& dataProvider)
			{
				ScopedCall eventLoopGuard([&] { eventLoop.exit(); });

				const auto db = m_databaseController->GetDatabase(true);
				const auto& book = dataProvider.GetBook();

				head = GetHead(*db, bookId, book.GetRawData(Flibrary::BookItem::Column::Title), root, self);

				const auto strategyCreator = FindSecond(ANNOTATION_CONTROLLER_STRATEGY_CREATORS, root.toStdString().data(), PszComparer {});
				const auto strategy = strategyCreator(*m_settings);
				auto annotation = m_annotationController->CreateAnnotation(dataProvider, *strategy);

				auto& entry = WriteEntry(root, head.children, book.GetId(), book.GetRawData(Flibrary::BookItem::Column::Title), 0, annotation, false);
				for (size_t i = 0, sz = dataProvider.GetAuthors().GetChildCount(); i < sz; ++i)
				{
					const auto& authorItem = dataProvider.GetAuthors().GetChild(i);
					auto& author = entry.children.emplace_back("author");
					author.children.emplace_back("name",
				                                 QString("%1 %2 %3")
				                                     .arg(authorItem->GetRawData(Flibrary::AuthorItem::Column::LastName),
				                                          authorItem->GetRawData(Flibrary::AuthorItem::Column::FirstName),
				                                          authorItem->GetRawData(Flibrary::AuthorItem::Column::MiddleName)));
					author.children.emplace_back("uri", QString("%1/%2/%3").arg(root, Loc::Authors, authorItem->GetId()));
				}
				for (size_t i = 0, sz = dataProvider.GetGenres().GetChildCount(); i < sz; ++i)
				{
					const auto& genreItem = dataProvider.GetGenres().GetChild(i);
					const auto& title = genreItem->GetRawData(Flibrary::NavigationItem::Column::Title);
					entry.children.emplace_back("category",
				                                QString {
                    },
				                                Node::Attributes { { "term", title }, { "label", title } });
				}
				const auto format = QFileInfo(book.GetRawData(Flibrary::BookItem::Column::FileName)).suffix();
				entry.children.emplace_back("dc:language", book.GetRawData(Flibrary::BookItem::Column::Lang));
				entry.children.emplace_back("dc:format", format);
				entry.children.emplace_back(
					"link",
					QString {
                },
					Node::Attributes { { "href", QString("%1/%2/data/%3").arg(root, BOOK, book.GetId()) }, { "rel", "http://opds-spec.org/acquisition" }, { "type", QString("application/%1").arg(format) } });
				entry.children.emplace_back("link",
			                                QString {
                },
			                                Node::Attributes { { "href", QString("%1/%2/zip/%3").arg(root, BOOK, book.GetId()) },
			                                                   { "rel", "http://opds-spec.org/acquisition" },
			                                                   { "type", QString("application/%1+zip").arg(format) } });
				m_forwarder.Forward([this] { m_coversTimer.start(); });
				std::lock_guard lock(m_coversGuard);

				if (const auto& covers = dataProvider.GetCovers(); !covers.empty())
				{
					entry.children.emplace_back("link",
				                                QString {
                    },
				                                Node::Attributes { { "href", QString("%1/%2/cover/%3").arg(root, BOOK, book.GetId()) }, { "rel", "http://opds-spec.org/image" }, { "type", "image/jpeg" } });

					entry.children.emplace_back(
						"link",
						QString {
                    },
						Node::Attributes { { "href", QString("%1/%2/cover/thumbnail/%3").arg(root, BOOK, book.GetId()) }, { "rel", "http://opds-spec.org/image/thumbnail" }, { "type", "image/jpeg" } });

					m_covers.try_emplace(bookId, covers.front().bytes);
				}
			});

		m_annotationController->RegisterObserver(&observer);
		m_annotationController->SetCurrentBookId(bookId, true);
		eventLoop.exec();

		return head;
	}

	QByteArray GetCoverThumbnail(const QString& bookId) const
	{
		{
			std::lock_guard lock(m_coversGuard);
			if (const auto it = m_covers.find(bookId); it != m_covers.end())
			{
				auto result = std::move(it->second);
				m_covers.erase(it);
				return result;
			}
		}

		QEventLoop eventLoop;
		QByteArray result;

		AnnotationControllerObserver observer(
			[&](const Flibrary::IAnnotationController::IDataProvider& dataProvider)
			{
				ScopedCall eventLoopGuard([&] { eventLoop.exit(); });
				if (const auto& covers = dataProvider.GetCovers(); !covers.empty())
					if (const auto coverIndex = dataProvider.GetCoverIndex())
					{
						result = covers[*coverIndex].bytes;
						return;
					}

				QFile file(":/images/book.png");
				[[maybe_unused]] const auto ok = file.open(QIODevice::ReadOnly);
				assert(ok);
				result = file.readAll();
			});

		m_annotationController->RegisterObserver(&observer);
		m_annotationController->SetCurrentBookId(bookId, true);
		eventLoop.exec();

		return result;
	}

	std::tuple<QString, QString, QByteArray> GetBookImpl(const QString& bookId, const bool restoreImages) const
	{
		auto book = GetExtractedBook(bookId);
		auto outputFileName = GetFileName(book);
		auto data = Decompress(m_collectionProvider->GetActiveCollection().folder, book.folder, book.file, restoreImages);

		return std::make_tuple(std::move(book.file), QFileInfo(outputFileName).fileName(), std::move(data));
	}

	std::pair<QString, QByteArray> GetBook(const QString& bookId, const bool restoreImages) const
	{
		auto [fileName, title, data] = GetBookImpl(bookId, restoreImages);
		return std::make_pair(title, std::move(data));
	}

	std::pair<QString, QByteArray> GetBookZip(const QString& bookId, const bool restoreImages) const
	{
		auto [fileName, title, data] = GetBookImpl(bookId, restoreImages);
		data = Compress(std::move(data), fileName);
		return std::make_pair(QFileInfo(title).completeBaseName() + ".zip", std::move(data));
	}

	QByteArray GetBookText(const QString& bookId) const
	{
		return std::get<2>(GetBookImpl(bookId, true));
	}

	Node WriteAuthorsNavigation(const QString& root, const QString& self, const QString& value) const
	{
		const auto startsWithQuery =
			QString("select %2, count(42) from Authors a where %1 a.SearchName != ? and a.SearchName like ? group by %2").arg(ShowRemoved() ? "" : "IsDeleted = 0 and").arg("substr(a.SearchName, %1, 1)");
		const auto navigationItemQuery = QString(R"(
select a.AuthorID, a.LastName || ' ' || a.FirstName || ' ' || a.MiddleName, count(42)
	from Authors a join Author_List l on l.AuthorID = a.AuthorID 
	join Books b on b.BookID = l.BookID
	left join Books_User bu on bu.BookID = b.BookID
	where %1 a.SearchName %2 ?
	group by a.AuthorID
)")
		                                     .arg(ShowRemoved() ? "" : QString("%1 and").arg(BOOKS_NOT_DELETED), "%1");
		return WriteNavigationStartsWith(*m_databaseController->GetDatabase(true), value, Loc::Authors, root, self, startsWithQuery, navigationItemQuery, &WriteNavigationEntries);
	}

	Node WriteSeriesNavigation(const QString& root, const QString& self, const QString& value) const
	{
		const auto startsWithQuery =
			QString("select %2, count(42) from Series a where %1 a.SearchTitle != ? and a.SearchTitle like ? group by %2").arg(ShowRemoved() ? "" : "IsDeleted = 0 and").arg("substr(a.SearchTitle, %1, 1)");
		const auto navigationItemQuery = QString(R"(
select a.SeriesID, a.SeriesTitle, count(42)
	from Series a 
	join Series_List sl on sl.SeriesID = a.SeriesID 
	join Books b on b.BookID = sl.BookID 
	left join Books_User bu on bu.BookID = b.BookID
	where %1 a.SearchTitle %2 ? group by a.SeriesID
)")
		                                     .arg(ShowRemoved() ? "" : QString("%1 and").arg(BOOKS_NOT_DELETED), "%1");
		return WriteNavigationStartsWith(*m_databaseController->GetDatabase(true), value, Loc::Series, root, self, startsWithQuery, navigationItemQuery, &WriteNavigationEntries);
	}

	void WriteGenresNavigationImpl(const QString& root, Node::Children& children, const QString& value) const
	{
		auto genres = Flibrary::Genre::Load(*m_databaseController->GetDatabase(true));
		auto* genre = Flibrary::Genre::Find(&genres, value);
		assert(genre);
		while (genre->children.size() == 1)
			genre = genre->children.data();

		if (!ShowRemoved())
			std::erase_if(genre->children, [](const auto& item) { return item.removed; });

		for (const auto& child : genre->children)
		{
			const auto id = QString("%1/starts/%2").arg(Loc::Genres).arg(child.code);
			WriteEntry(root, children, id, child.name, -1);
		}

		auto node = WriteGenresAuthors(root, QString(), genre->code, QString {});
		std::ranges::move(std::move(node.children) | std::views::filter([](const auto& item) { return item.name == ENTRY; }), std::back_inserter(children));
	}

	Node WriteGenresNavigation(const QString& root, const QString& self, const QString& value) const
	{
		const auto db = m_databaseController->GetDatabase(true);
		auto head = GetHead(*db, Loc::Genres, value.isEmpty() ? Loc::Tr(Loc::NAVIGATION, Loc::Genres) : QString("%1/%2").arg(Loc::Genres, value), root, self);
		WriteGenresNavigationImpl(root, head.children, value);
		return head;
	}

	Node WriteKeywordsNavigation(const QString& root, const QString& self, const QString& value) const
	{
		const auto startsWithQuery =
			QString("select %2, count(42) from Keywords a where %1 a.SearchTitle != ? and a.SearchTitle like ? group by %2").arg(ShowRemoved() ? "" : "IsDeleted = 0 and").arg("substr(a.SearchTitle, %1, 1)");
		const auto navigationItemQuery = QString(R"(
select a.KeywordID, a.KeywordTitle, count(42) 
	from Keywords a 
	join Keyword_List l on l.KeywordID = a.KeywordID
	join Books b on b.BookID = l.BookID 
	left join Books_User bu on bu.BookID = b.BookID
	where %1 a.SearchTitle %2 ? 
	group by a.KeywordID
)")
		                                     .arg(ShowRemoved() ? "" : QString("%1 and").arg(BOOKS_NOT_DELETED), "%1");
		return WriteNavigationStartsWith(*m_databaseController->GetDatabase(true), value, Loc::Keywords, root, self, startsWithQuery, navigationItemQuery, &WriteNavigationEntries);
	}

	Node WriteArchivesNavigation(const QString& root, const QString& self, const QString& /*value*/) const
	{
		const auto db = m_databaseController->GetDatabase(true);
		auto head = GetHead(*db, Loc::Archives, Loc::Tr(Loc::NAVIGATION, Loc::Archives), root, self);
		const auto queryText = QString(R"(
select f.FolderID, f.FolderTitle, count(42)
	from Folders f
	join Books b on b.FolderID = f.FolderID
	left join Books_User bu on bu.BookID = b.BookID
	%1 group by f.FolderID
)")
		                           .arg(ShowRemoved() ? "" : QString("where %1").arg(BOOKS_NOT_DELETED));
		const auto query = db->CreateQuery(queryText.toStdString());
		for (query->Execute(); !query->Eof(); query->Next())
		{
			const auto id = QString("%1/%2").arg(Loc::Archives).arg(query->Get<int>(0));
			WriteEntry(root, head.children, id, query->Get<const char*>(1), query->Get<int>(2));
		}

		return head;
	}

	Node WriteGroupsNavigation(const QString& /*root*/, const QString& /*self*/, const QString& /*value*/) const
	{
		assert(false && "Unexpected call");
		return {};
	}

	Node WriteAuthorsAuthors(const QString& root, const QString& self, const QString& navigationId, const QString& value) const
	{
		return WriteAuthorsAuthorBooks(root, self, navigationId, {}, value);
	}

	Node WriteAuthorsAuthorBooks(const QString& root, const QString& self, const QString& navigationId, const QString& authorId, const QString& value) const
	{
		return WriteAuthorBooksImpl(root, self, navigationId, authorId, value, Loc::Authors, JOIN_AUTHOR);
	}

	Node WriteSeriesAuthors(const QString& root, const QString& self, const QString& navigationId, const QString& value) const
	{
		return WriteAuthorsImpl(root, self, navigationId, value, Loc::Series, JOIN_SERIES);
	}

	Node WriteSeriesAuthorBooks(const QString& root, const QString& self, const QString& navigationId, const QString& authorId, const QString& value) const
	{
		return WriteAuthorBooksImpl(root, self, navigationId, authorId, value, Loc::Series, JOIN_SERIES);
	}

	Node WriteGenresAuthors(const QString& root, const QString& self, const QString& navigationId, const QString& value) const
	{
		return WriteAuthorsImpl(root, self, navigationId, value, Loc::Genres, JOIN_GENRE);
	}

	Node WriteGenresAuthorBooks(const QString& root, const QString& self, const QString& navigationId, const QString& authorId, const QString& value) const
	{
		return WriteAuthorBooksImpl(root, self, navigationId, authorId, value, Loc::Genres, JOIN_GENRE);
	}

	Node WriteKeywordsAuthors(const QString& root, const QString& self, const QString& navigationId, const QString& value) const
	{
		return WriteAuthorsImpl(root, self, navigationId, value, Loc::Keywords, JOIN_KEYWORD);
	}

	Node WriteKeywordsAuthorBooks(const QString& root, const QString& self, const QString& navigationId, const QString& authorId, const QString& value) const
	{
		return WriteAuthorBooksImpl(root, self, navigationId, authorId, value, Loc::Keywords, JOIN_KEYWORD);
	}

	Node WriteArchivesAuthors(const QString& root, const QString& self, const QString& navigationId, const QString& value) const
	{
		return WriteAuthorsImpl(root, self, navigationId, value, Loc::Archives, JOIN_ARCHIVE);
	}

	Node WriteArchivesAuthorBooks(const QString& root, const QString& self, const QString& navigationId, const QString& authorId, const QString& value) const
	{
		return WriteAuthorBooksImpl(root, self, navigationId, authorId, value, Loc::Archives, JOIN_ARCHIVE);
	}

	Node WriteGroupsAuthors(const QString& root, const QString& self, const QString& navigationId, const QString& value) const
	{
		return WriteAuthorsImpl(root, self, navigationId, value, Loc::Groups, JOIN_GROUP);
	}

	Node WriteGroupsAuthorBooks(const QString& root, const QString& self, const QString& navigationId, const QString& authorId, const QString& value) const
	{
		return WriteAuthorBooksImpl(root, self, navigationId, authorId, value, Loc::Groups, JOIN_GROUP);
	}

	QJsonObject getConfig(const QString&) const
	{
		const auto db = m_databaseController->GetDatabase(true);

		QJsonObject result;
		{
			auto query = db->CreateQuery("select count (42) from Books");
			query->Execute();
			assert(!query->Eof());
			result.insert("numberOfBooks", query->Get<long long>(0));
		}

		{
			QJsonArray array;
			const auto genres = Flibrary::Genre::Load(*db);
			const auto process = [&](const Flibrary::Genre& genre, const auto& f) -> void
			{
				for (const auto& child : genre.children)
				{
					array.append(QJsonObject {
						{  "GenreCode",                child.code },
						{ "ParentCode",                genre.code },
						{    "FB2Code",             child.fb2Code },
						{ "GenreAlias",                child.name },
						{  "IsDeleted", child.removed ? "1" : "0" },
					});
					f(child, f);
				}
			};

			process(genres, process);
			result.insert("genres", array);
		}

		return result;
	}

	QJsonObject getSearchStats(const QString& searchTerms) const
	{
		static constexpr auto queryText = R"(
with Search (Title) as (
    select ?
)
select (select count (42) from Books_Search join Search s on Books_Search match s.Title) as bookTitles
    , (select count (42) from Authors_Search join Search s on Authors_Search match s.Title) as authors
    , (select count (42) from Series_Search join Search s on Series_Search match s.Title) as bookSeries
)";
		const auto query = CreateSearchQuery(queryText, searchTerms);
		query->Execute();
		assert(!query->Eof());

		return QJsonObject {
			{ "searchStats", FromQuery<int>(*query) }
		};
	}

	QJsonObject getSearchTitles(const QString& searchTerms) const
	{
		static constexpr auto queryText = R"(
select b.BookID, b.Title, b.BookSize, b.Lang, b.LibRate, s.SeriesTitle, nullif(b.SeqNumber, 0) as SeqNumber, b.FileName, (
    select group_concat(a.LastName || coalesce(' ' || nullif(a.FirstName, ''), '') || coalesce(' ' || nullif(a.MiddleName, ''), ''), ', ')
        from Authors a 
        join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID
    ) as AuthorsNames, (
    select group_concat(g.GenreAlias, ', ')
        from Genres g
        join Genre_List gl on gl.GenreCode = g.GenreCode and gl.BookID = b.BookID
    ) as Genres
from Books b
join Books_Search fts on fts.rowid = b.BookID and Books_Search match ?
left join Series s on s.SeriesID = b.SeriesID
)";
		return GetBookListBySearch(queryText, searchTerms, "titlesList");
	}

	QJsonObject getSearchAuthors(const QString& searchTerms) const
	{
		static constexpr auto queryText = R"(
select a.AuthorID, a.LastName || coalesce(' ' || nullif(a.FirstName, ''), '') || coalesce(' ' || nullif(a.MiddleName, ''), '') as Authors, count(42) as Books
from Authors a
join Authors_Search fts on fts.rowid = a.AuthorID and Authors_Search match ?
join Author_List al on al.AuthorID = a.AuthorID
group by a.AuthorID
)";
		return GetBookListBySearch(queryText, searchTerms, "authorsList");
	}

	QJsonObject getSearchSeries(const QString& searchTerms) const
	{
		static constexpr auto queryText = R"(
select s.SeriesID, s.SeriesTitle, count(42) as Books
from Series s
join Series_Search fts on fts.rowid = s.SeriesID and Series_Search match ?
join Series_List sl on sl.SeriesID = s.SeriesID
group by s.SeriesID
)";
		return GetBookListBySearch(queryText, searchTerms, "seriesList");
	}

	QJsonObject getSearchAuthorBooks(const QString& authorId) const
	{
		static constexpr auto queryText = R"(
select b.BookID, b.Title, b.BookSize, b.Lang, b.LibRate, s.SeriesTitle, nullif(b.SeqNumber, 0) as SeqNumber, (
    select group_concat(g.GenreAlias, ', ')
        from Genres g
        join Genre_List gl on gl.GenreCode = g.GenreCode and gl.BookID = b.BookID
    ) as Genres
from Books b
join Author_List al on al.BookID = b.BookID and al.AuthorID = ?
left join Series s on s.SeriesID = b.SeriesID
)";
		return GetBookListById(queryText, authorId, "titlesList");
	}

	QJsonObject getSearchSeriesBooks(const QString& seriesId) const
	{
		static constexpr auto queryText = R"(
select b.BookID, b.Title, b.BookSize, b.Lang, b.LibRate, nullif(b.SeqNumber, 0) as SeqNumber, (
    select group_concat(a.LastName || coalesce(' ' || nullif(a.FirstName, ''), '') || coalesce(' ' || nullif(a.MiddleName, ''), ''), ', ')
        from Authors a 
        join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID
    ) as AuthorsNames, (
    select group_concat(g.GenreAlias, ', ')
        from Genres g
        join Genre_List gl on gl.GenreCode = g.GenreCode and gl.BookID = b.BookID
    ) as Genres
from Books b
join Series_List sl on sl.BookID = b.BookID and sl.SeriesID = ?
)";
		return GetBookListById(queryText, seriesId, "titlesList");
	}

	QJsonObject getBookForm(const QString& bookId) const
	{
		QJsonObject result;

		QEventLoop eventLoop;
		AnnotationControllerObserver observer(
			[&](const Flibrary::IAnnotationController::IDataProvider& dataProvider)
			{
				ScopedCall eventLoopGuard([&] { eventLoop.exit(); });

				const auto& book = dataProvider.GetBook();

				QJsonArray authors;

				const auto authorsList = [&]
				{
					QStringList values;
					for (size_t i = 0, sz = dataProvider.GetAuthors().GetChildCount(); i < sz; ++i)
					{
						const auto& authorItem = dataProvider.GetAuthors().GetChild(i);

						authors.append(QJsonObject {
							{   "AuthorID",											  authorItem->GetId() },
							{  "FirstName",  authorItem->GetRawData(Flibrary::AuthorItem::Column::FirstName) },
							{   "LastName",   authorItem->GetRawData(Flibrary::AuthorItem::Column::LastName) },
							{ "MiddleName", authorItem->GetRawData(Flibrary::AuthorItem::Column::MiddleName) },
						});

						values << QString("%1 %2 %3")
									  .arg(authorItem->GetRawData(Flibrary::AuthorItem::Column::LastName),
					                       authorItem->GetRawData(Flibrary::AuthorItem::Column::FirstName),
					                       authorItem->GetRawData(Flibrary::AuthorItem::Column::MiddleName))
									  .split(' ', Qt::SkipEmptyParts)
									  .join(' ');
					}
					return values.join(", ");
				}();

				const auto genresList = [&]
				{
					QStringList values;
					for (size_t i = 0, sz = dataProvider.GetGenres().GetChildCount(); i < sz; ++i)
						values << dataProvider.GetGenres().GetChild(i)->GetRawData(Flibrary::NavigationItem::Column::Title);
					return values.join(", ");
				}();

				QJsonArray series;
				const auto& bookSeries = dataProvider.GetSeries();
				for (size_t i = 0, sz = bookSeries.GetChildCount(); i < sz; ++i)
				{
					const auto item = bookSeries.GetChild(i);
					QJsonObject obj {
						{    "SeriesID",										 item->GetId() },
						{ "SeriesTitle", item->GetRawData(Flibrary::SeriesItem::Column::Title) },
					};
					if (const auto seqNum = Util::Fb2InpxParser::GetSeqNumber(item->GetRawData(Flibrary::SeriesItem::Column::SeqNum)); !seqNum.isEmpty())
						obj.insert("SeqNumber", seqNum);
					series.append(obj);
				}

				QJsonObject bookForm {
					{       "BookID",																			  book.GetId() },
					{     "BookSize",										 book.GetRawData(Flibrary::BookItem::Column::Size) },
					{     "FileName",               QFileInfo(book.GetRawData(Flibrary::BookItem::Column::FileName)).baseName() },
					{          "Ext",           "." + QFileInfo(book.GetRawData(Flibrary::BookItem::Column::FileName)).suffix() },
					{         "Lang",										 book.GetRawData(Flibrary::BookItem::Column::Lang) },
					{      "LibRate",									  book.GetRawData(Flibrary::BookItem::Column::LibRate) },
					{    "SeqNumber", Util::Fb2InpxParser::GetSeqNumber(book.GetRawData(Flibrary::BookItem::Column::SeqNumber)) },
					{  "SeriesTitle",									   book.GetRawData(Flibrary::BookItem::Column::Series) },
					{        "Title",										book.GetRawData(Flibrary::BookItem::Column::Title) },
					{ "AuthorsNames",																			   authorsList },
					{       "Genres",																				genresList },
				};

				result.insert("annotation", dataProvider.GetAnnotation());
				result.insert("city", dataProvider.GetPublishCity());
				result.insert("isbn", dataProvider.GetPublishIsbn());
				result.insert("publisher", dataProvider.GetPublisher());
				result.insert("year", dataProvider.GetPublishYear());

				result.insert("authors", authors);
				result.insert("bookForm", bookForm);
				result.insert("series", series);

				m_forwarder.Forward([this] { m_coversTimer.start(); });
				std::lock_guard lock(m_coversGuard);

				if (const auto& covers = dataProvider.GetCovers(); !covers.empty())
					m_covers.try_emplace(bookId, covers.front().bytes);
			});

		m_annotationController->RegisterObserver(&observer);
		m_annotationController->SetCurrentBookId(bookId, true);
		eventLoop.exec();

		return result;
	}

private: // IPostProcessCallback
	QString GetFileName(const QString& bookId) const override
	{
		const auto book = GetExtractedBook(bookId);
		return GetFileName(book);
	}

	std::pair<QString, std::vector<QByteArray>> GetAuthorInfo(const QString& name) const override
	{
		if (auto info = m_authorAnnotationController->GetInfo(name); !info.isEmpty())
			return std::make_pair(std::move(info), m_authorAnnotationController->GetImages(name));

		return {};
	}

private:
	std::unique_ptr<DB::IQuery> CreateSearchQuery(const QString& queryText, const QString& searchTerms) const
	{
		const auto db = m_databaseController->GetDatabase(true);
		auto query = db->CreateQuery(queryText.toStdString());
		query->Bind(0, PrepareSearchTerms(searchTerms).toStdString());
		return query;
	}

	std::unique_ptr<DB::IQuery> CreateIdQuery(const QString& queryText, const QString& id) const
	{
		const auto db = m_databaseController->GetDatabase(true);
		auto query = db->CreateQuery(queryText.toStdString());
		bool ok = false;
		query->Bind(0, id.toLongLong(&ok));
		assert(ok);
		return query;
	}

	QJsonObject GetBookListBySearch(const QString& queryText, const QString& searchTerms, const QString& resultName) const
	{
		QJsonArray array;

		const auto query = CreateSearchQuery(queryText, searchTerms);
		for (query->Execute(); !query->Eof(); query->Next())
			array.append(FromQuery<const char*>(*query));

		return QJsonObject {
			{ resultName, array }
		};
	}

	QJsonObject GetBookListById(const QString& queryText, const QString& id, const QString& resultName) const
	{
		QJsonArray array;

		auto query = CreateIdQuery(queryText, id);
		for (query->Execute(); !query->Eof(); query->Next())
			array.append(FromQuery<const char*>(*query));

		return QJsonObject {
			{ resultName, array }
		};
	}

	QString GetFileName(const Flibrary::ILogicFactory::ExtractedBook& book) const
	{
		auto outputFileName = m_outputFileNameTemplate;
		Flibrary::ILogicFactory::FillScriptTemplate(outputFileName, book);
		if (!m_settings->Get(OPDS_TRANSLITERATE, false))
			return outputFileName;

		Transliterate("ru-ru_Latn/BGN", outputFileName);
		Transliterate("Any-Latin", outputFileName);
		Transliterate("Latin-ASCII", outputFileName);

		return outputFileName;
	}

	Flibrary::ILogicFactory::ExtractedBook GetExtractedBook(const QString& bookId) const
	{
		const auto db = m_databaseController->GetDatabase(true);
		const auto query = db->CreateQuery(R"(
select
    f.FolderTitle,
    b.FileName||b.Ext,
    b.BookSize,
    (select a.LastName ||' '||a.FirstName ||' '||a.MiddleName from Authors a join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID order by al.AuthorID limit 1),
    s.SeriesTitle,
    b.SeqNumber,
    b.Title
from Books b
join Folders f on f.FolderID = b.FolderID
left join Series s on s.SeriesID = b.SeriesID
where b.BookID = ?
)");
		query->Bind(0, bookId.toLongLong());
		query->Execute();
		if (query->Eof())
			return {};

		return Flibrary::ILogicFactory::ExtractedBook { bookId.toInt(),           query->Get<const char*>(0), query->Get<const char*>(1),
			                                            query->Get<long long>(2), query->Get<const char*>(3), query->Get<const char*>(4),
			                                            query->Get<int>(5),       query->Get<const char*>(6) };
	}

	Node WriteAuthorsImpl(const QString& root, const QString& self, const QString& navigationId, const QString& value, const QString& type, QString join, const bool checkBooksQuery = true) const
	{
		const auto db = m_databaseController->GetDatabase(true);
		join = join.arg(navigationId);

		if (checkBooksQuery)
		{
			auto node = WriteBooksList(root, self, navigationId, type, join);
			if (!node.children.empty())
				return node;
		}

		const auto startsWithQuery = QString(SELECT_AUTHORS_STARTS_WITH).arg(join, "%1");
		const auto bookItemQuery = QString(SELECT_AUTHORS).arg(join, "%1");
		const auto navigationType = QString("%1/%2").arg(type, navigationId).toStdString();
		auto node = WriteNavigationStartsWith(*db, value, navigationType.data(), root, self, startsWithQuery, bookItemQuery, &WriteNavigationEntries);
		if (value.isEmpty()
		    && std::ranges::any_of(node.children,
		                           [n = 0](const Node& item) mutable
		                           {
									   n += item.name == ENTRY;
									   return n > 1;
								   }))
		{
			const auto query = db->CreateQuery(QString(SELECT_BOOK_COUNT).arg(join, ShowRemoved() ? "" : QString(" where %1").arg(BOOKS_NOT_DELETED)).toStdString());
			query->Execute();
			if (const auto count = query->Get<int>(0))
				WriteEntry(root, node.children, QString("%1/Books/%2").arg(type, navigationId), Tr(BOOKS), count);
		}
		return node;
	}

	Node WriteAuthorBooksImpl(const QString& root, const QString& self, const QString& navigationId, const QString& authorId, const QString& value, const QString& type, QString join) const
	{
		if (!join.isEmpty())
			join = join.arg(navigationId);

		if (!authorId.isEmpty())
			join.append("\n").append(QString(JOIN_AUTHOR).arg(authorId));

		if (value.isEmpty())
		{
			auto node = WriteBooksList(root, self, navigationId, type, join);
			if (!node.children.empty())
				return node;
		}

		const auto isNotDeleted = ShowRemoved() ? "" : QString(" and %1").arg(BOOKS_NOT_DELETED);
		const auto startsWithQuery = QString(SELECT_BOOKS_STARTS_WITH).arg(join, "%1", isNotDeleted);
		const auto bookItemQuery = QString(SELECT_BOOKS).arg(join, SELECT_BOOKS_WHERE).append(isNotDeleted);
		const auto navigationType = (authorId.isEmpty() ? QString("%1/Books/%2").arg(type, navigationId) : QString("%1/Authors/Books/%2/%3").arg(type, navigationId, authorId)).toStdString();
		return WriteNavigationStartsWith(*m_databaseController->GetDatabase(true), value, navigationType.data(), root, self, startsWithQuery, bookItemQuery, &WriteBookEntries);
	}

	Node WriteBooksList(const QString& root, const QString& self, const QString& navigationId, const QString& type, const QString& join) const
	{
		const auto db = m_databaseController->GetDatabase(true);
		if (navigationId.isEmpty() ||
		    [&]
		    {
				const auto query = db->CreateQuery(QString(SELECT_BOOK_COUNT).arg(join, ShowRemoved() ? "" : QString(" where %1").arg(BOOKS_NOT_DELETED)).toStdString());
				query->Execute();
				assert(!query->Eof());
				const auto n = query->Get<long long>(0);
				return n == 0 || n > GetMaxResultSize();
			}())
			return Node {};

		const auto isNotDeleted = ShowRemoved() ? "" : QString("where %1").arg(BOOKS_NOT_DELETED);
		auto head = GetHead(*db, navigationId, QString("%1/%2").arg(type, navigationId), root, self);
		WriteBookEntries(*db, "", QString("%1%2").arg(QString(SELECT_BOOKS).arg(join, isNotDeleted)), navigationId, root, head.children);
		const auto it = std::ranges::find(head.children, ENTRY, [](const auto& item) { return item.name; });
		std::sort(it, head.children.end());
		return head;
	}

	bool ShowRemoved() const
	{
		return m_settings->Get(Flibrary::Constant::Settings::SHOW_REMOVED_BOOKS_KEY, false);
	}

	ptrdiff_t GetMaxResultSize() const
	{
		if (const auto result = m_settings->Get(OPDS_BOOK_LIMIT_KEY, OPDS_BOOK_LIMIT_DEFAULT); result > 0)
			return result;
		return OPDS_BOOK_LIMIT_DEFAULT;
	}

private:
	std::shared_ptr<const ISettings> m_settings;
	std::shared_ptr<const Flibrary::ICollectionProvider> m_collectionProvider;
	std::shared_ptr<const Flibrary::IDatabaseController> m_databaseController;
	std::shared_ptr<const Flibrary::IAuthorAnnotationController> m_authorAnnotationController;
	std::shared_ptr<Flibrary::IAnnotationController> m_annotationController;
	Util::FunctorExecutionForwarder m_forwarder;
	const QString m_outputFileNameTemplate;
	mutable QTimer m_coversTimer;
	mutable std::mutex m_coversGuard;
	mutable std::unordered_map<QString, QByteArray> m_covers;
};

namespace
{

QByteArray PostProcess(const ContentType contentType, const QString& root, const IPostProcessCallback& callback, QByteArray& src, const QStringList& parameters)
{
	if (root.isEmpty())
		return src;

	QBuffer buffer(&src);
	buffer.open(QIODevice::ReadOnly);
	const auto postprocessor = FindSecond(POSTPROCESSORS, root.toStdString().data(), PszComparer {});
	auto result = postprocessor(callback, buffer, contentType, parameters);

#ifndef NDEBUG
	PLOGV << result;
#endif

	return result;
}

template <typename Obj, typename NavigationGetter, typename... ARGS>
QByteArray GetImpl(Obj& obj, NavigationGetter getter, const ContentType contentType, const QString& root, const QString& self, const ARGS&... args)
{
	if (!obj.GetCollectionProvider().ActiveCollectionExists())
		return {};

	QByteArray bytes;
	QBuffer buffer(&bytes);
	try
	{
		const ScopedCall bufferGuard([&] { buffer.open(QIODevice::WriteOnly); }, [&] { buffer.close(); });
		const auto node = std::invoke(getter, std::cref(obj), std::cref(root), std::cref(self), std::cref(args)...);
		Util::XmlWriter writer(buffer);
		writer << node;
	}
	catch (const std::exception& ex)
	{
		PLOGE << ex.what();
		return {};
	}
	catch (...)
	{
		PLOGE << "Unknown error";
		return {};
	}
	buffer.close();

#ifndef NDEBUG
	PLOGV << bytes;
#endif

	return PostProcess(contentType, root, obj, bytes, { root });
}

template <typename Obj, typename NavigationGetter, typename... ARGS>
QByteArray GetImpl(Obj& obj, NavigationGetter getter, const ARGS&... args)
{
	if (!obj.GetCollectionProvider().ActiveCollectionExists())
		return {};

	QByteArray bytes;
	try
	{
		const QJsonDocument doc(std::invoke(getter, std::cref(obj), std::cref(args)...));
		bytes = doc.toJson();
	}
	catch (const std::exception& ex)
	{
		PLOGE << ex.what();
		return {};
	}
	catch (...)
	{
		PLOGE << "Unknown error";
		return {};
	}

#ifndef NDEBUG
	PLOGV << bytes;
#endif

	return bytes;
}

} // namespace

Requester::Requester(std::shared_ptr<const ISettings> settings,
                     std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
                     std::shared_ptr<const Flibrary::IDatabaseController> databaseController,
                     std::shared_ptr<const Flibrary::IAuthorAnnotationController> authorAnnotationController,
                     std::shared_ptr<Flibrary::IAnnotationController> annotationController)
	: m_impl(std::move(settings), std::move(collectionProvider), std::move(databaseController), std::move(authorAnnotationController), std::move(annotationController))
{
	PLOGV << "Requester created";
}

Requester::~Requester()
{
	PLOGV << "Requester destroyed";
}

QByteArray Requester::GetRoot(const QString& root, const QString& self) const
{
	return GetImpl(*m_impl, &Impl::WriteRoot, ContentType::Root, root, self);
}

QByteArray Requester::GetBookInfo(const QString& root, const QString& self, const QString& bookId) const
{
	return GetImpl(*m_impl, &Impl::WriteBook, ContentType::BookInfo, root, self, bookId);
}

QByteArray Requester::GetCover(const QString& /*root*/, const QString& /*self*/, const QString& bookId) const
{
	return m_impl->GetCoverThumbnail(bookId);
}

QByteArray Requester::GetCoverThumbnail(const QString& root, const QString& self, const QString& bookId) const
{
	return GetCover(root, self, bookId);
}

std::pair<QString, QByteArray> Requester::GetBook(const QString& /*root*/, const QString& /*self*/, const QString& bookId, const bool restoreImages) const
{
	return m_impl->GetBook(bookId, restoreImages);
}

std::pair<QString, QByteArray> Requester::GetBookZip(const QString& /*root*/, const QString& /*self*/, const QString& bookId, const bool restoreImages) const
{
	return m_impl->GetBookZip(bookId, restoreImages);
}

QByteArray Requester::GetBookText(const QString& root, const QString& bookId) const
{
	auto result = m_impl->GetBookText(bookId);
	return PostProcess(ContentType::BookText, root, *m_impl, result, { root, bookId });
}

QByteArray Requester::Search(const QString& root, const QString& self, const QString& searchTerms, const QString& start) const
{
	return GetImpl(*m_impl, &Impl::WriteSearch, ContentType::Books, root, self, searchTerms, start);
}

#define OPDS_ROOT_ITEM(NAME)                                                                                          \
	QByteArray Requester::Get##NAME##Navigation(const QString& root, const QString& self, const QString& value) const \
	{                                                                                                                 \
		return GetImpl(*m_impl, &Impl::Write##NAME##Navigation, ContentType::Navigation, root, self, value);          \
	}
OPDS_ROOT_ITEMS_X_MACRO
#undef OPDS_ROOT_ITEM

#define OPDS_ROOT_ITEM(NAME)                                                                                                                    \
	QByteArray Requester::Get##NAME##Authors(const QString& root, const QString& self, const QString& navigationId, const QString& value) const \
	{                                                                                                                                           \
		return GetImpl(*m_impl, &Impl::Write##NAME##Authors, ContentType::Authors, root, self, navigationId, value);                            \
	}
OPDS_ROOT_ITEMS_X_MACRO
#undef OPDS_ROOT_ITEM

#define OPDS_ROOT_ITEM(NAME)                                                                                                                                                 \
	QByteArray Requester::Get##NAME##AuthorBooks(const QString& root, const QString& self, const QString& navigationId, const QString& authorId, const QString& value) const \
	{                                                                                                                                                                        \
		return GetImpl(*m_impl, &Impl::Write##NAME##AuthorBooks, ContentType::Books, root, self, navigationId, authorId, value);                                             \
	}
OPDS_ROOT_ITEMS_X_MACRO
#undef OPDS_ROOT_ITEM

#define OPDS_GET_BOOKS_API_ITEM(NAME, _)                   \
	QByteArray Requester::NAME(const QString& value) const \
	{                                                      \
		return GetImpl(*m_impl, &Impl::NAME, value);       \
	}
OPDS_GET_BOOKS_API_ITEMS_X_MACRO
#undef OPDS_GET_BOOKS_API_ITEM
