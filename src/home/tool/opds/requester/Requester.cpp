#include "Requester.h"

#include <ranges>

#include <QBuffer>
#include <QByteArray>
#include <QEventLoop>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#include "fnd/FindPair.h"
#include "fnd/ScopedCall.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/Enums.h"
#include "interface/constants/GenresLocalization.h"
#include "interface/constants/Localization.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseController.h"
#include "interface/logic/IDatabaseUser.h"

#include "logic/data/DataItem.h"
#include "logic/data/Genre.h"
#include "util/AnnotationControllerObserver.h"
#include "util/Fb2InpxParser.h"
#include "util/SortString.h"
#include "util/localization.h"
#include "util/timer.h"
#include "util/xml/XmlWriter.h"

#include "log.h"
#include "root.h"

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

constexpr auto CONTEXT = "Requester";
constexpr auto BOOKS_COUNTER = QT_TRANSLATE_NOOP("Requester", "Books: %1");
constexpr auto BOOKS = QT_TRANSLATE_NOOP("Requester", "Books");

constexpr auto SEARCH_RESULTS = QT_TRANSLATE_NOOP("Requester", R"(Books found for the request "%1": %2)");
constexpr auto NOTHING_FOUND = QT_TRANSLATE_NOOP("Requester", R"(No books found for the request "%1")");
constexpr auto PREVIOUS = QT_TRANSLATE_NOOP("Requester", "[Previous page]");
constexpr auto NEXT = QT_TRANSLATE_NOOP("Requester", "[Next page]");
constexpr auto FIRST = QT_TRANSLATE_NOOP("Requester", "[First page]");
constexpr auto LAST = QT_TRANSLATE_NOOP("Requester", "[Last page]");

constexpr auto BOOK = "BookInfo";
constexpr auto ENTRY = "entry";
constexpr auto TITLE = "title";
constexpr auto CONTENT = "content";
constexpr auto SEARCH = "search";
constexpr auto START = "start";
constexpr auto STARTS = "starts";
constexpr auto OPDS_BOOK_LIMIT_KEY = "opds/BookEntryLimit";
constexpr auto OPDS_BOOK_LIMIT_DEFAULT = 25;

TR_DEF

#define FULL_AUTHOR_NAME " a.LastName || coalesce(' ' || nullif(a.FirstName, '') || coalesce(' ' || nullif(a.middleName, ''), ''), '') "

constexpr auto AUTHOR_COUNT = "with WithTable(Id) as (select distinct l.AuthorID from Author_List l %1) select '%2', count(42) from WithTable";
constexpr auto SERIES_COUNT = "with WithTable(Id) as (select distinct l.SeriesID from Series_List l %1) select '%2', count(42) from WithTable";
constexpr auto GENRE_COUNT = "with WithTable(Id) as (select distinct l.GenreCode from Genre_List l %1) select '%2', count(42) from WithTable";
constexpr auto KEYWORD_COUNT = "with WithTable(Id) as (select distinct l.KeywordID from Keyword_List l %1) select '%2', count(42) from WithTable";
constexpr auto UPDATE_COUNT = "with WithTable(Id) as (select distinct l.UpdateID from Books l  %1) select '%2', count(42) from WithTable";
constexpr auto FOLDER_COUNT = "with WithTable(Id) as (select distinct l.FolderID from Books l  %1) select '%2', count(42) from WithTable";
constexpr auto BOOK_COUNT = "select count(42) from Books l %1";

constexpr auto AUTHOR_JOIN_PARAMETERS = "join Author_List al on al.BookID = l.BookID and al.AuthorID = %1";
constexpr auto SERIES_JOIN_PARAMETERS = "join Series_List sl on sl.BookID = l.BookID and sl.SeriesID = %1";
constexpr auto GENRE_JOIN_PARAMETERS = "join Genre_List gl on gl.BookID = l.BookID and gl.GenreCode = '%1'";
constexpr auto KEYWORD_JOIN_PARAMETERS = "join Keyword_List kl on kl.BookID = l.BookID and kl.KeywordID = %1";
constexpr auto UPDATE_JOIN_PARAMETERS = "join Books ul on ul.BookID = l.BookID and ul.UpdateID = %1";
constexpr auto FOLDER_JOIN_PARAMETERS = "join Books fl on fl.BookID = l.BookID and fl.FolderID = %1";
constexpr auto GROUP_JOIN_PARAMETERS = "join Groups_List_User_View glu on glu.BookID = l.BookID and glu.GroupID = %1";

constexpr auto AUTHOR_JOIN_SELECT = "join Author_List l on l.AuthorID = a.AuthorID";
constexpr auto SERIES_JOIN_SELECT = "join Series_List l on l.SeriesID = s.SeriesID";
constexpr auto GENRE_JOIN_SELECT = "join Genre_List l on l.GenreCode = g.GenreCode";
constexpr auto KEYWORD_JOIN_SELECT = "join Keyword_List l on l.KeywordID = k.KeywordID";
constexpr auto UPDATE_JOIN_SELECT = "join Books l on l.UpdateID = u.UpdateID";
constexpr auto FOLDER_JOIN_SELECT = "join Books l on l.FolderID = f.FolderID";
constexpr auto GROUP_JOIN_SELECT = "join Groups_List_User_View glu on glu.GroupID = gu.GroupID";
constexpr auto BOOK_JOIN_SELECT = "from Books l";

constexpr auto AUTHOR_SELECT = "select" FULL_AUTHOR_NAME "from Authors a where a.AuthorID = ?";
constexpr auto SERIES_SELECT = "select s.SeriesTitle from Series s where s.SeriesID = ?";
constexpr auto GENRE_SELECT = "select g.FB2Code from Genres g where g.GenreCode = ?";
constexpr auto KEYWORD_SELECT = "select k.KeywordTitle from Keywords k where k.KeywordID = ?";
constexpr auto UPDATE_SELECT = "select u.UpdateTitle, p.UpdateTitle from Updates u left join Updates p on p.UpdateID = u.ParentID where u.UpdateID = ?";
constexpr auto FOLDER_SELECT = "select f.FolderTitle from Folders f where f.FolderID = ?";
constexpr auto GROUP_SELECT = "select gu.Title from Groups_User gu where gu.GroupID = ?";

constexpr auto AUTHOR_COUNT_STARTS_WITH = R"(
	select count(distinct a.AuthorID) 
	from Authors a 
	%3 
	where a.IsDeleted != %1 and (a.SearchName = :starts or a.SearchName like :starts_like||'%' ESCAPE '%2')
)";

constexpr auto SERIES_COUNT_STARTS_WITH = R"(
	select count(distinct s.SeriesID) 
	from Series s 
	%3 
	where s.IsDeleted != %1 and (s.SearchTitle = :starts or s.SearchTitle like :starts_like||'%' ESCAPE '%2')
)";

constexpr auto KEYWORD_COUNT_STARTS_WITH = R"(
	select count(distinct k.KeywordID) 
	from Keywords k 
	%3 
	where k.IsDeleted != %1 and (k.SearchTitle = :starts or k.SearchTitle like :starts_like||'%' ESCAPE '%2')
)";

constexpr auto UPDATE_COUNT_STARTS_WITH = "select 0";
constexpr auto FOLDER_COUNT_STARTS_WITH = "select 0";

constexpr auto BOOK_COUNT_STARTS_WITH = R"(
	select count(42) 
	%3 
	where b.IsDeleted != %1 and (b.SearchTitle = :starts or b.SearchTitle like :starts_like||'%' ESCAPE '%2')
)";

constexpr auto AUTHOR_STARTS_WITH = R"(
	select substr(a.SearchName, 1, :length), count(distinct a.AuthorID) 
	from Authors a 
	%3 
	where a.IsDeleted != %1 and (a.SearchName = :starts or a.SearchName like :starts_like||'%' ESCAPE '%2')
	group by substr(a.SearchName, 1, :length)
)";

constexpr auto SERIES_STARTS_WITH = R"(
	select substr(s.SearchTitle, 1, :length), count(distinct s.SeriesID) 
	from Series s 
	%3 
	where s.IsDeleted != %1 and (s.SearchTitle = :starts or s.SearchTitle like :starts_like||'%' ESCAPE '%2')
	group by substr(s.SearchTitle, 1, :length)
)";

constexpr auto KEYWORD_STARTS_WITH = R"(
	select substr(k.SearchTitle, 1, :length), count(distinct k.KeywordID) 
	from Keywords k 
	%3 
	where k.IsDeleted != %1 and (k.SearchTitle = :starts or k.SearchTitle like :starts_like||'%' ESCAPE '%2')
	group by substr(k.SearchTitle, 1, :length)
)";

constexpr auto BOOK_STARTS_WITH = R"(
	select substr(b.SearchTitle, 1, :length), count(42) 
	%3 
	where b.IsDeleted != %1 and (b.SearchTitle = :starts or b.SearchTitle like :starts_like||'%' ESCAPE '%2')
	group by substr(b.SearchTitle, 1, :length)
)";

constexpr auto GROUPS_SELECT = R"(
	select g.GroupID, g.Title, count(42)
	from Groups_User g 
	join Groups_List_User_View l on l.GroupID = g.GroupID 
	%2
	where g.IsDeleted != %1 
	group by g.GroupID
)";

constexpr auto AUTHOR_SELECT_SINGLE =
	"select distinct a.AuthorID," FULL_AUTHOR_NAME "from Authors a %3 where a.IsDeleted != %1 and (a.SearchName = :starts or a.SearchName like :starts_like||'%' ESCAPE '%2')";
constexpr auto SERIES_SELECT_SINGLE = "select distinct s.SeriesID, s.SeriesTitle from Series s %3 where s.IsDeleted != %1 and (s.SearchTitle = :starts or s.SearchTitle like :starts_like||'%' ESCAPE '%2')";
constexpr auto KEYWORD_SELECT_SINGLE =
	"select distinct k.KeywordID, k.KeywordTitle from Keywords k %3 where k.IsDeleted != %1 and (k.SearchTitle = :starts or k.SearchTitle like :starts_like||'%' ESCAPE '%2')";
constexpr auto FOLDER_SELECT_SINGLE = "select f.FolderID, f.FolderTitle from Folders f %3 where f.IsDeleted != %1 and ('%2' = '%2' and :starts = :starts and :starts_like = :starts_like)";
constexpr auto BOOK_SELECT_SINGLE = "select l.BookID, l.Title %3 where b.IsDeleted != %1 and (b.SearchTitle = :starts or b.SearchTitle like :starts_like||'%' ESCAPE '%2')";

constexpr auto AUTHOR_SELECT_EQUAL = "select distinct a.AuthorID," FULL_AUTHOR_NAME "from Authors a %2 where a.IsDeleted != %1 and a.SearchName = :starts";
constexpr auto SERIES_SELECT_EQUAL = "select distinct s.SeriesID, s.SeriesTitle from Series s %2 where s.IsDeleted != %1 and s.SearchTitle = :starts";
constexpr auto GENRE_SELECT_EQUAL = "select g.GenreCode from Genres g where exists (select 42 from Genre_List l %1 where l.GenreCode = g.GenreCode)";
constexpr auto KEYWORD_SELECT_EQUAL = "select distinct k.KeywordID, k.KeywordTitle from Keywords k %2 where k.IsDeleted != %1 and k.SearchTitle = :starts";
constexpr auto UPDATE_SELECT_EQUAL = "select u.UpdateID from Updates u where exists (select 42 from Books l %1 where l.UpdateID = u.UpdateID)";
constexpr auto BOOK_SELECT_EQUAL = "select b.BookID, b.Title %2 where b.IsDeleted != %1 and b.SearchTitle = :starts";

constexpr auto AUTHOR_CONTENT = "select count (42) from Authors a %1 where l.AuthorID = ?";
constexpr auto SERIES_CONTENT = "select count (42) from Series s %1 where l.SeriesID = ?";
constexpr auto GENRE_CONTENT = "select count(42) from Genre_List l %1 where l.GenreCode = ?";
constexpr auto KEYWORD_CONTENT = "select count (42) from Keywords k %1 where k.KeywordID = ?";
constexpr auto UPDATE_CONTENT = "select count (42) from Books l %1 where l.UpdateID = ?";
constexpr auto FOLDER_CONTENT = "select count (42) from Folders f %1 where f.FolderID = ?";
constexpr auto BOOK_CONTENT = R"(
select 
	(select a.LastName || coalesce(' ' || nullif(substr(a.FirstName, 1, 1), '') || '.' || coalesce(nullif(substr(a.middleName, 1, 1), '') || '.', ''), '')
		from Authors a
		join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID ORDER BY a.ROWID ASC LIMIT 1
	), s.SeriesTitle, b.SeqNumber
	from Books b
	left join Series s on s.SeriesID = b.SeriesID
	where b.BookID = ?
)";

constexpr auto SEARCH_QUERY_TEXT = R"(
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
select b.BookID, b.Title
	from Books_View b
	join Ids i on i.BookID = b.BookID
	where b.IsDeleted != %1
)";

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

QString& GetContent(Node& node)
{
	const auto it = std::ranges::find(node.children, CONTENT, [](auto& item) { return item.name; });
	assert(it != node.children.end());
	return it->value;
}

const QString& GetContent(const Node& node)
{
	return GetContent(const_cast<Node&>(node));
}

std::tuple<Util::QStringWrapper, Util::QStringWrapper, int, Util::QStringWrapper> ToComparable(const Node& node)
{
	return node.author.isEmpty() ? std::make_tuple(Util::QStringWrapper { node.title }, Util::QStringWrapper { GetContent(node) }, 0, Util::QStringWrapper { node.title })
	                             : std::make_tuple(Util::QStringWrapper { node.author }, Util::QStringWrapper { node.series }, node.seqNum, Util::QStringWrapper { node.title });
}

bool operator<(const Node& lhs, const Node& rhs)
{
	return ToComparable(lhs) < ToComparable(rhs);
}

Util::XmlWriter& operator<<(Util::XmlWriter& writer, const Node& node)
{
	const auto nodeGuard = writer.Guard(node.name);
	std::ranges::for_each(node.attributes, [&](const auto& item) { writer.WriteAttribute(item.first, item.second); });
	writer.WriteCharacters(node.value);
	std::ranges::for_each(node.children, [&](const auto& item) { writer << item; });
	return writer;
}

QString GetHref(const QString& root, const QString& path, const IRequester::Parameters& parameters)
{
	QStringList list;
	list.reserve(static_cast<int>(parameters.size()));
	std::ranges::transform(parameters, std::back_inserter(list), [](const auto& item) { return QString("%1=%2").arg(item.first, item.second); });
	return QString("%1%2%3").arg(root, path.isEmpty() ? QString {} : "/" + path, list.isEmpty() ? QString {} : "?" + list.join('&'));
}

std::vector<Node> GetStandardNodes(QString id, QString title)
{
	return std::vector<Node> {
		{ "updated", QDateTime::currentDateTime().toUTC().toString("yyyy-MM-ddThh:mm:ssZ") },
		{      "id",														 std::move(id) },
		{     TITLE,													  std::move(title) },
	};
}

Node& WriteEntry(Node::Children& children, const QString& root, const QString& path, const IRequester::Parameters& parameters, QString id, QString title, QString content = {}, const bool isCatalog = true)
{
	auto& entry = children.emplace_back(ENTRY, QString {}, Node::Attributes {}, GetStandardNodes(std::move(id), title));
	entry.title = std::move(title);

	if (isCatalog)
	{
		const auto href = GetHref(root, path, parameters);
		entry.children.emplace_back("link",
		                            QString {
        },
		                            Node::Attributes { { "href", QUrl(href).toString(QUrl::FullyEncoded) }, { "rel", "subsection" }, { "type", "application/atom+xml;profile=opds-catalog;kind=navigation" } });
	}
	if (!content.isEmpty())
		entry.children.emplace_back(CONTENT,
		                            std::move(content),
		                            Node::Attributes {
										{ "type", "text/html" }
        });

	return entry;
}

struct NavigationDescription;
void WriteBookEntries(Node::Children&, const QString&, IRequester::Parameters, const NavigationDescription&, DB::IDatabase& b, bool, QString, std::map<QString, QString>&);
void WriteNavigationEntries(Node::Children&, const QString&, IRequester::Parameters, const NavigationDescription&, DB::IDatabase& b, bool, QString, std::map<QString, QString>&);

class INavigationProvider // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~INavigationProvider() = default;
	virtual Node GetNavigationMain(const QString& root, const IRequester::Parameters& parameters, const NavigationDescription& d) const = 0;
	virtual Node GetNavigationGenre(const QString& root, const IRequester::Parameters& parameters, const NavigationDescription& d) const = 0;
	virtual Node GetNavigationUpdate(const QString& root, const IRequester::Parameters& parameters, const NavigationDescription& d) const = 0;
};

using GetNavigation = Node (INavigationProvider::*)(const QString& root, const IRequester::Parameters& parameters, const NavigationDescription& d) const;
using WriteEntries = void (*)(Node::Children& children,
                              const QString& root,
                              IRequester::Parameters parameters,
                              const NavigationDescription& d,
                              DB::IDatabase& db,
                              bool removedFlag,
                              QString join,
                              std::map<QString, QString>& ones);

struct NavigationDescription
{
	const char* type { nullptr };
	const char* count { nullptr };
	const char* joinParameters { nullptr };
	const char* joinSelect { nullptr };
	const char* select { nullptr };
	const char* selectTrContext { nullptr };
	const char* countStartsWith { nullptr };
	const char* startsWith { nullptr };
	const char* selectSingle { nullptr };
	const char* selectEqual { nullptr };
	const char* content { nullptr };
	GetNavigation getNavigation { &INavigationProvider::GetNavigationMain };
	WriteEntries writeEntries { &WriteNavigationEntries };
};

// clang-format off
constexpr NavigationDescription NAVIGATION_DESCRIPTION[] {
	{ Loc::Authors  ,  AUTHOR_COUNT,  AUTHOR_JOIN_PARAMETERS,  AUTHOR_JOIN_SELECT,  AUTHOR_SELECT,         nullptr,  AUTHOR_COUNT_STARTS_WITH,  AUTHOR_STARTS_WITH,  AUTHOR_SELECT_SINGLE,  AUTHOR_SELECT_EQUAL,  AUTHOR_CONTENT  },
	{ Loc::Series   ,  SERIES_COUNT,  SERIES_JOIN_PARAMETERS,  SERIES_JOIN_SELECT,  SERIES_SELECT,         nullptr,  SERIES_COUNT_STARTS_WITH,  SERIES_STARTS_WITH,  SERIES_SELECT_SINGLE,  SERIES_SELECT_EQUAL,  SERIES_CONTENT  },
	{ Loc::Genres   ,   GENRE_COUNT,   GENRE_JOIN_PARAMETERS,   GENRE_JOIN_SELECT,   GENRE_SELECT, Flibrary::GENRE,                   nullptr,             nullptr,               nullptr,   GENRE_SELECT_EQUAL,   GENRE_CONTENT, &INavigationProvider::GetNavigationGenre },
	{ Loc::Keywords , KEYWORD_COUNT, KEYWORD_JOIN_PARAMETERS, KEYWORD_JOIN_SELECT, KEYWORD_SELECT,         nullptr, KEYWORD_COUNT_STARTS_WITH, KEYWORD_STARTS_WITH, KEYWORD_SELECT_SINGLE, KEYWORD_SELECT_EQUAL, KEYWORD_CONTENT },
	{ Loc::Updates  ,  UPDATE_COUNT,  UPDATE_JOIN_PARAMETERS,  UPDATE_JOIN_SELECT,  UPDATE_SELECT,  MONTHS_CONTEXT,  UPDATE_COUNT_STARTS_WITH,             nullptr,               nullptr,  UPDATE_SELECT_EQUAL,  UPDATE_CONTENT, &INavigationProvider::GetNavigationUpdate },
	{ Loc::Archives ,  FOLDER_COUNT,  FOLDER_JOIN_PARAMETERS,  FOLDER_JOIN_SELECT,  FOLDER_SELECT,         nullptr,  FOLDER_COUNT_STARTS_WITH,             nullptr,  FOLDER_SELECT_SINGLE,              nullptr,  FOLDER_CONTENT  },
	{ Loc::Languages },
	{ Loc::Groups   ,       nullptr,   GROUP_JOIN_PARAMETERS,   GROUP_JOIN_SELECT,   GROUP_SELECT },
	{ Loc::Search },
	{ Loc::AllBooks },
};
// clang-format on
static_assert(std::size(NAVIGATION_DESCRIPTION) == static_cast<size_t>(Flibrary::NavigationMode::Last));

constexpr NavigationDescription BOOK_DESCRIPTION {
	BOOKS, BOOK_COUNT, nullptr, BOOK_JOIN_SELECT, nullptr, nullptr, BOOK_COUNT_STARTS_WITH, BOOK_STARTS_WITH, BOOK_SELECT_SINGLE, BOOK_SELECT_EQUAL, BOOK_CONTENT, nullptr, &WriteBookEntries
};

QString GetSeriesTitle(const QString& title, QString seqNum)
{
	seqNum = Util::Fb2InpxParser::GetSeqNumber(seqNum);
	return QString("%1%2").arg(title, seqNum.isEmpty() ? QString {} : " #" + seqNum);
}

QString CreateSelf(const QString& root, const QString& path, const IRequester::Parameters& parameters)
{
	QStringList list;
	list.reserve(static_cast<int>(parameters.size()));
	std::ranges::transform(parameters, std::back_inserter(list), [](const auto& item) { return QString("%1=%2").arg(item.first, item.second); });
	return QString("%1%2%3").arg(root, path.isEmpty() ? QString {} : QString("/%1").arg(path), list.isEmpty() ? QString {} : "?" + list.join('&'));
}

void WriteBookEntries(Node::Children& children, const QString& root, IRequester::Parameters parameters, const NavigationDescription& d, DB::IDatabase& db, bool, QString, std::map<QString, QString>& ones)
{
	const auto needAuthor = !parameters.contains(Loc::Authors);
	for (auto&& [navigationId, title] : ones)
	{
		parameters[BOOKS] = navigationId;

		const auto query = db.CreateQuery(d.content);
		query->Bind(0, navigationId.toStdString());
		query->Execute();
		assert(!query->Eof());
		QString author = query->Get<const char*>(0);
		QString series = query->Get<const char*>(1);
		const int seqNum = query->Get<int>(2);
		auto content = QString("%1 %2").arg(needAuthor ? author : QString {}, GetSeriesTitle(series, QString::number(seqNum)));

		auto& entry = WriteEntry(children, root, BOOK, parameters, QString("%1/%2").arg(d.type, navigationId), std::move(title), content.simplified(), false);

		auto href = CreateSelf(root, BOOK, parameters);

		entry.children.emplace_back(
			"link",
			QString {
        },
			Node::Attributes { { "href", QUrl(href).toString(QUrl::FullyEncoded) }, { "rel", "subsection" }, { "type", "application/atom+xml;profile=opds-catalog;kind=acquisition" } });

		entry.author = std::move(author);
		entry.series = std::move(series);
		entry.seqNum = seqNum > 0 ? seqNum : std::numeric_limits<int>::max();
	}
}

void WriteNavigationEntries(Node::Children& children,
                            const QString& root,
                            IRequester::Parameters parameters,
                            const NavigationDescription& d,
                            DB::IDatabase& db,
                            const bool removedFlag,
                            QString join,
                            std::map<QString, QString>& ones)
{
	if (join.isEmpty())
		join = QString("join Books_View b on b.BookID = l.BookID and b.IsDeleted != %1\n%2").arg(removedFlag).arg(d.joinSelect);

	for (auto&& [navigationId, title] : ones)
	{
		const auto query = db.CreateQuery(QString(d.content).arg(join).toStdString());
		query->Bind(0, navigationId.toStdString());
		query->Execute();
		assert(!query->Eof());
		parameters[d.type] = navigationId;
		WriteEntry(children, root, "", parameters, QString("%1/%2").arg(d.type, navigationId), std::move(title), Tr(BOOKS_COUNTER).arg(query->Get<long long>(0)));
	}
}

std::pair<QString, char> PrepareForLike(QString arg)
{
	static constexpr char ESCAPE[] { '\\', '|', '#', '@', '~', '^' };
	const auto it = std::ranges::find_if(ESCAPE, [&](const auto ch) { return !arg.contains(ch); });
	assert(it != std::end(ESCAPE));
	arg.replace('_', QString("%1_").arg(*it));
	arg.replace('%', QString("%1%").arg(*it));
	return std::make_pair(std::move(arg), *it);
}

Node GetHead(QString id, QString title, QString root, QString self)
{
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

QString GetParameter(const IRequester::Parameters& parameters, const QString& key)
{
	const auto it = parameters.find(key);
	return it != parameters.end() ? it->second : QString {};
}

QString GetJoin(const IRequester::Parameters& parameters)
{
	if (parameters.empty())
		return {};

	QStringList list;
	std::ranges::transform(NAVIGATION_DESCRIPTION | std::views::filter([&](const auto& item) { return item.joinParameters && parameters.contains(item.type); }),
	                       std::back_inserter(list),
	                       [&](const auto& item) { return QString(item.joinParameters).arg(parameters.at(item.type)); });
	if (list.isEmpty())
		return {};

	list << "join Books_View b on b.BookID = l.BookID and b.IsDeleted != %1";

	return list.join("\n");
}

struct TitleHelper
{
	QString defaultTitle;
	QString additionalTitle;
};

QString GetTitle(DB::IDatabase& db, const IRequester::Parameters& parameters, TitleHelper helper)
{
	QStringList list;
	std::ranges::transform(NAVIGATION_DESCRIPTION | std::views::filter([&](const auto& item) { return item.select && parameters.contains(item.type); }),
	                       std::back_inserter(list),
	                       [&](const auto& item)
	                       {
							   const auto query = db.CreateQuery(item.select);
							   query->Bind(0, parameters.at(item.type).toStdString());
							   query->Execute();
							   assert(!query->Eof());
							   auto result = item.selectTrContext ? QString(Loc::Tr(item.selectTrContext, query->template Get<const char*>(0))) : query->template Get<const char*>(0);
							   if (query->ColumnCount() > 1)
								   result.append(' ').append(query->template Get<const char*>(1));
							   return result;
						   });
	if (!helper.additionalTitle.isEmpty())
		list.emplaceBack(std::move(helper.additionalTitle));

	return list.isEmpty() ? std::move(helper.defaultTitle) : list.join(", ");
}

template <typename T>
QString ToString(T value)
{
	return QString::number(value);
}

template <>
QString ToString<QString>(QString value)
{
	return value;
}

template <typename T>
T ToItemCode(const QString& value)
{
	return static_cast<T>(value.toLongLong());
}

template <>
QString ToItemCode<QString>(const QString& value)
{
	return value;
}

template <typename T>
void BindImpl(DB::IQuery& query, const size_t index, const T& value)
{
	query.Bind(index, value);
}

template <>
void BindImpl<QString>(DB::IQuery& query, const size_t index, const QString& value)
{
	query.Bind(index, value.toStdString());
}

template <typename T>
QString GetContent(const T& item)
{
	return QString::number(item.children.size());
}

template <>
QString GetContent<Flibrary::Update>(const Flibrary::Update&)
{
	return {};
}

} // namespace

class Requester::Impl final
	: public IPostProcessCallback
	, INavigationProvider
{
private: // IPostProcessCallback
	QString GetFileName(const QString& bookId) const override
	{
		return m_bookExtractor->GetFileName(bookId);
	}

	std::pair<QString, std::vector<QByteArray>> GetAuthorInfo(const QString& name) const override
	{
		if (auto info = m_authorAnnotationController->GetInfo(name); !info.isEmpty())
			return std::make_pair(std::move(info), m_authorAnnotationController->GetImages(name));

		return {};
	}

public:
	Impl(std::shared_ptr<const ISettings> settings,
	     std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
	     std::shared_ptr<const Flibrary::IDatabaseController> databaseController,
	     std::shared_ptr<const Flibrary::IAuthorAnnotationController> authorAnnotationController,
	     std::shared_ptr<const ICoverCache> coverCache,
	     std::shared_ptr<const IBookExtractor> bookExtractor,
	     std::shared_ptr<const INoSqlRequester> noSqlRequester,
	     std::shared_ptr<Flibrary::IAnnotationController> annotationController)
		: m_settings { std::move(settings) }
		, m_collectionProvider { std::move(collectionProvider) }
		, m_databaseController { std::move(databaseController) }
		, m_authorAnnotationController { std::move(authorAnnotationController) }
		, m_coverCache { std::move(coverCache) }
		, m_bookExtractor { std::move(bookExtractor) }
		, m_noSqlRequester { std::move(noSqlRequester) }
		, m_annotationController { std::move(annotationController) }
	{
	}

	const Flibrary::ICollectionProvider& GetCollectionProvider() const
	{
		return *m_collectionProvider;
	}

	Node GetRoot(const QString& root, const Parameters& parameters) const
	{
		const auto removedFlag = GetRemovedFlag();
		const auto db = m_databaseController->GetDatabase(true);

		static constexpr std::pair<Flibrary::NavigationMode, const char*> navigationTypes[] {
#define OPDS_INVOKER_ITEM(NAME) { Flibrary::NavigationMode::NAME, Loc::NAME },
			OPDS_NAVIGATION_ITEMS_X_MACRO
#undef OPDS_INVOKER_ITEM
		};

		auto head = GetHead("root", GetTitle(*db, parameters, { .defaultTitle = m_collectionProvider->GetActiveCollection().name }), root, CreateSelf(root, "", parameters));

		auto join = GetJoin(parameters);
		if (!join.isEmpty())
			join = join.arg(removedFlag);

		QStringList dbStatQueryTextItems;
		std::ranges::transform(navigationTypes | std::views::filter([&](const auto& item) { return NAVIGATION_DESCRIPTION[static_cast<size_t>(item.first)].count && !parameters.contains(item.second); }),
		                       std::back_inserter(dbStatQueryTextItems),
		                       [&](const auto& item)
		                       {
								   const auto& d = NAVIGATION_DESCRIPTION[static_cast<size_t>(item.first)];
								   return QString(d.count).arg(join).arg(d.type);
							   });

		{
			if (!parameters.empty())
				WriteEntry(head.children, root, BOOKS, parameters, BOOKS, Tr(BOOKS), "Stub");

			const auto childCount = head.children.size();
			for (const auto& dbStatQueryText : dbStatQueryTextItems)
			{
				const auto query = db->CreateQuery(dbStatQueryText.toStdString());
				query->Execute();
				assert(!query->Eof());
				const auto* id = query->Get<const char*>(0);
				if (const auto count = query->Get<int>(1); count > 1)
					WriteEntry(head.children, root, id, parameters, id, Loc::Tr(Loc::NAVIGATION, id), QString::number(count));
			}
			if (!parameters.empty())
			{
				if (childCount == head.children.size())
					return GetBooks(root, parameters);

				const auto query = db->CreateQuery(QString(BOOK_DESCRIPTION.count).arg(join).toStdString());
				query->Execute();
				assert(!query->Eof());

				auto& books = head.children[childCount - 1];
				GetContent(books) = QString::number(query->Get<long long>(0));
			}
		}

		if (!parameters.contains(Loc::Groups))
		{
			const auto query = db->CreateQuery(QString(GROUPS_SELECT).arg(removedFlag).arg(join).toStdString());
			for (query->Execute(); !query->Eof(); query->Next())
			{
				auto p = parameters;
				const auto id = QString("%1/%2").arg(Loc::Groups).arg(p[Loc::Groups] = query->Get<const char*>(0));
				WriteEntry(head.children, root, "", p, id, query->Get<const char*>(1), Tr(BOOKS_COUNTER).arg(query->Get<int>(2)));
			}
		}

		return head;
	}

	Node GetBooks(const QString& root, const Parameters& parameters) const
	{
		const auto db = m_databaseController->GetDatabase(true);

		auto head = GetHead(BOOKS, GetTitle(*db, parameters, {}), root, CreateSelf(root, BOOKS, parameters));
		FoldNavigation(head.children, root, parameters, BOOK_DESCRIPTION);

		const auto entryBegin = std::ranges::find(head.children, ENTRY, [](const auto& item) { return item.name; });
		std::sort(entryBegin, head.children.end());

		return head;
	}

	Node GetBookInfo(const QString& root, const Parameters& parameters) const
	{
		Node head;

		QEventLoop eventLoop;

		AnnotationControllerObserver observer(
			[&](const Flibrary::IAnnotationController::IDataProvider& dataProvider)
			{
				ScopedCall eventLoopGuard([&] { eventLoop.exit(); });

				const auto db = m_databaseController->GetDatabase(true);
				const auto& book = dataProvider.GetBook();

				head = GetHead(book.GetId(), book.GetRawData(Flibrary::BookItem::Column::Title), root, CreateSelf(root, BOOKS, parameters));

				const auto strategyCreator = FindSecond(ANNOTATION_CONTROLLER_STRATEGY_CREATORS, root.toStdString().data(), PszComparer {});
				const auto strategy = strategyCreator(*m_settings);
				auto annotation = m_annotationController->CreateAnnotation(dataProvider, *strategy);

				auto& entry = WriteEntry(head.children, root, "", parameters, book.GetId(), book.GetRawData(Flibrary::BookItem::Column::Title), annotation, false);
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
					Node::Attributes { { "href", QString("/Images/fb2/%1").arg(book.GetId()) }, { "rel", "http://opds-spec.org/acquisition" }, { "type", QString("application/%1").arg(format) } });
				entry.children.emplace_back(
					"link",
					QString {
                },
					Node::Attributes { { "href", QString("/Images/zip/%1").arg(book.GetId()) }, { "rel", "http://opds-spec.org/acquisition" }, { "type", QString("application/%1+zip").arg(format) } });

				if (const auto& covers = dataProvider.GetCovers(); !covers.empty())
				{
					entry.children.emplace_back("link",
				                                QString {
                    },
				                                Node::Attributes { { "href", QString("/Images/covers/%1").arg(book.GetId()) }, { "rel", "http://opds-spec.org/image" }, { "type", "image/jpeg" } });

					entry.children.emplace_back("link",
				                                QString {
                    },
				                                Node::Attributes { { "href", QString("/Images/covers/%1").arg(book.GetId()) }, { "rel", "http://opds-spec.org/image/thumbnail" }, { "type", "image/jpeg" } });

					m_coverCache->Set(book.GetId(), covers.front().bytes);
				}
			});

		m_annotationController->RegisterObserver(&observer);
		m_annotationController->SetCurrentBookId(parameters.at(BOOKS), true);
		eventLoop.exec();

		return head;
	}

	Node GetNavigation(const QString& root, const Parameters& parameters, const Flibrary::NavigationMode navigationMode) const
	{
		const auto& d = NAVIGATION_DESCRIPTION[static_cast<size_t>(navigationMode)];
		return std::invoke(d.getNavigation, static_cast<const INavigationProvider*>(this), std::cref(root), std::cref(parameters), std::cref(d));
	}

	Node Search(const QString& root, const Parameters& parameters) const
	{
		const auto removedFlag = GetRemovedFlag();
		const auto db = m_databaseController->GetDatabase(true);
		const auto searchTerms = parameters.at("q");
		const auto start = [&]
		{
			const auto it = parameters.find(START);
			return it != parameters.end() ? it->second : QString {};
		}();

		auto terms = searchTerms.split(QRegularExpression(R"(\s+|\+)"), Qt::SkipEmptyParts);
		const auto termsGui = terms.join(' ');
		std::ranges::transform(terms, terms.begin(), [](const auto& item) { return item + '*'; });

		auto head = GetHead(SEARCH, Tr(NOTHING_FOUND).arg(termsGui), root, CreateSelf(root, SEARCH, parameters));

		const auto n = head.children.size();

		std::map<QString, QString> ones;
		{
			const auto query = db->CreateQuery(QString(SEARCH_QUERY_TEXT).arg(removedFlag).toStdString());
			query->Bind(0, terms.join(' ').toStdString());
			for (query->Execute(); !query->Eof(); query->Next())
				ones.try_emplace(query->Get<const char*>(0), query->Get<const char*>(1));
		}
		{
			auto p = parameters;
			p.erase("q");
			WriteBookEntries(head.children, root, std::move(p), BOOK_DESCRIPTION, *db, false, {}, ones);
		}

		if (n != head.children.size())
		{
			const auto it = std::ranges::find(head.children, TITLE, [](const auto& item) { return item.name; });
			assert(it != head.children.end());
			it->value = Tr(SEARCH_RESULTS).arg(termsGui).arg(head.children.size() - n);
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
				auto p = parameters;
				p[START] = QString::number(nextPageIndex);
				const QUrl url(CreateSelf(root, SEARCH, p));
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

	QByteArray GetBookText(const QString& bookId) const
	{
		return m_noSqlRequester->GetBook(bookId).second;
	}

private: // INavigationProvider
	Node GetNavigationMain(const QString& root, const Parameters& parameters, const NavigationDescription& d) const override
	{
		const auto db = m_databaseController->GetDatabase(true);
		auto head = GetHead(d.type, GetTitle(*db, parameters, { .additionalTitle = Loc::Tr(Loc::NAVIGATION, d.type) }), root, CreateSelf(root, d.type, parameters));
		FoldNavigation(head.children, root, parameters, d);

		const auto entryBegin = std::ranges::find(head.children, ENTRY, [](const auto& item) { return item.name; });
		std::sort(entryBegin, head.children.end(), [](const auto& lhs, const auto& rhs) { return Util::QStringWrapper { lhs.title } < Util::QStringWrapper { rhs.title }; });

		return head;
	}

	template <typename T, typename DBCodeType>
	Node GetNavigationT(const QString& root, const Parameters& parameters, const NavigationDescription& d) const
	{
		const auto removedFlag = GetRemovedFlag();
		const auto db = m_databaseController->GetDatabase(true);

		auto join = [&]
		{
			auto p = parameters;
			p.erase(d.type);
			auto result = GetJoin(p).arg(removedFlag);
			if (result.isEmpty())
				return QString("join Books_View b on b.BookID = l.BookID and b.IsDeleted != %1").arg(removedFlag);
			return result;
		}();

		std::unordered_set<typename T::CodeType> itemsWithBooks;
		if (!parameters.empty() && (parameters.size() > 1 || parameters.begin()->first != d.type))
		{
			const auto query = db->CreateQuery(QString(d.selectEqual).arg(join).toStdString());
			for (query->Execute(); !query->Eof(); query->Next())
				itemsWithBooks.emplace(query->template Get<DBCodeType>(0));
		}

		auto rootItem = T::Load(*db, itemsWithBooks);
		const auto* parentItem = &rootItem;
		if (const auto it = parameters.find(d.type); it != parameters.end())
			parentItem = T::Find(parentItem, ToItemCode<typename T::CodeType>(it->second));

		TitleHelper titleHelper { .defaultTitle = Loc::Tr(Loc::NAVIGATION, d.type) };
		if (!parameters.contains(d.type))
			titleHelper.additionalTitle = titleHelper.defaultTitle;
		auto head = GetHead(d.type, GetTitle(*db, parameters, std::move(titleHelper)), root, CreateSelf(root, d.type, parameters));

		Parameters typedParameters = parameters;
		typedParameters.erase(STARTS);

		for (const auto& childItem : parentItem->children)
		{
			typedParameters[d.type] = ToString(childItem.code);
			auto [path, content] = [&]() -> std::pair<QString, QString>
			{
				if (!childItem.children.empty())
					return std::make_pair(QString(d.type), GetContent(childItem));

				const auto query = db->CreateQuery(QString(d.content).arg(join).toStdString());
				BindImpl(*query, 0, childItem.code);
				query->Execute();
				assert(!query->Eof());
				return std::make_pair(QString {}, Tr(BOOKS_COUNTER).arg(query->template Get<int>(0)));
			}();
			WriteEntry(head.children, root, path, typedParameters, QString("%1/%2").arg(d.type).arg(childItem.code), childItem.name, std::move(content));
		}

		return head;
	}

	Node GetNavigationGenre(const QString& root, const Parameters& parameters, const NavigationDescription& d) const override
	{
		return GetNavigationT<Flibrary::Genre, const char*>(root, parameters, d);
	}

	Node GetNavigationUpdate(const QString& root, const Parameters& parameters, const NavigationDescription& d) const override
	{
		return GetNavigationT<Flibrary::Update, long long>(root, parameters, d);
	}

private:
	void FoldNavigation(Node::Children& children, const QString& root, const Parameters& parameters, const NavigationDescription& d) const
	{
		const auto removedFlag = GetRemovedFlag();
		const auto db = m_databaseController->GetDatabase(true);

		ptrdiff_t equalCount = 0;
		QStringList equal;
		std::map<QString, QString> ones;
		std::multimap<long long, QString, std::greater<>> buffer;

		const auto startsWithGlobal = GetParameter(parameters, STARTS);
		auto join = GetJoin(parameters);
		if (!join.isEmpty())
			join = QString("%1\n%2").arg(d.joinSelect, join.arg(removedFlag));

		const auto countTotal = [&]
		{
			const auto [startsWithLike, escape] = PrepareForLike(startsWithGlobal);
			const auto queryText = QString(d.countStartsWith).arg(removedFlag).arg(escape).arg(join);
			const auto query = db->CreateQuery(queryText.toStdString());
			query->Bind(":starts", startsWithGlobal.toStdString());
			query->Bind(":starts_like", startsWithLike.toStdString());
			query->Execute();
			assert(!query->Eof());
			return query->Get<long long>(0);
		}();

		const auto selectOne = [&](const QString& queryText, const std::vector<std::pair<QString, QString>>& params)
		{
			const auto query = db->CreateQuery(queryText.toStdString());
			for (const auto& [key, value] : params)
				query->Bind(key.toStdString(), value.toStdString());

			for (query->Execute(); !query->Eof(); query->Next())
				ones.try_emplace(QString(query->Get<const char*>(0)).simplified(), query->Get<const char*>(1));
		};

		const auto maxSize = GetMaxResultSize();
		if (countTotal <= maxSize)
		{
			const auto [startsWithLike, escape] = PrepareForLike(startsWithGlobal);
			const auto queryText = QString(d.selectSingle).arg(removedFlag).arg(escape).arg(join);
			selectOne(queryText,
			          {
						  {      ":starts", startsWithGlobal },
						  { ":starts_like",   startsWithLike },
            });
		}
		else
		{
			const auto selectStarts = [&](const auto startsWith)
			{
				const auto [startsWithLike, escape] = PrepareForLike(startsWith);
				const auto queryText = QString(d.startsWith).arg(removedFlag).arg(escape).arg(join);
				const auto query = db->CreateQuery(queryText.toStdString());
				query->Bind(":length", startsWith.length() + 1);
				query->Bind(":starts", startsWith.toStdString());
				query->Bind(":starts_like", startsWithLike.toStdString());
				for (query->Execute(); !query->Eof(); query->Next())
				{
					QString s = query->template Get<const char*>(0);
					const auto count = query->template Get<long long>(1);
					if (s == startsWith)
					{
						equal.emplaceBack(std::move(s));
						equalCount += count;
					}
					else
					{
						buffer.emplace(count, std::move(s));
					}
				}
			};

			selectStarts(startsWithGlobal);

			while (!buffer.empty() && equalCount + static_cast<ptrdiff_t>(buffer.size()) < maxSize)
			{
				auto it = buffer.begin();
				const auto startsWith = std::move(it->second);
				buffer.erase(it);
				selectStarts(startsWith);
			}
		}

		for (auto&& [count, startsWith] : buffer)
		{
			Parameters p = parameters;
			const auto& s = (p[STARTS] = std::move(startsWith));
			WriteEntry(children, root, d.type, p, QString("%1/starts/%2").arg(d.type, s), QString("%1~").arg(s), QString::number(count));
		}

		for (const auto& s : equal)
		{
			const auto queryText = QString(d.selectEqual).arg(removedFlag).arg(join);
			selectOne(queryText,
			          {
						  { ":starts", s },
            });
		}

		Parameters typedParameters = parameters;
		typedParameters.erase(STARTS);
		d.writeEntries(children, root, std::move(typedParameters), d, *db, removedFlag, std::move(join), ones);
	}

	int GetRemovedFlag() const
	{
		return ShowRemoved() ? 2 : 1;
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
	std::shared_ptr<const ICoverCache> m_coverCache;
	std::shared_ptr<const IBookExtractor> m_bookExtractor;
	std::shared_ptr<const INoSqlRequester> m_noSqlRequester;
	std::shared_ptr<Flibrary::IAnnotationController> m_annotationController;
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
QByteArray GetImpl(const Obj& obj, NavigationGetter getter, const ContentType contentType, const QString& root, const IRequester::Parameters& parameters, const ARGS&... args)
{
	if (!obj.GetCollectionProvider().ActiveCollectionExists())
		return {};

	QByteArray bytes;
	QBuffer buffer(&bytes);
	try
	{
		const ScopedCall bufferGuard([&] { buffer.open(QIODevice::WriteOnly); }, [&] { buffer.close(); });
		const auto node = std::invoke(getter, std::cref(obj), std::cref(root), std::cref(parameters), std::cref(args)...);
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

} // namespace

Requester::Requester(std::shared_ptr<const ISettings> settings,
                     std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
                     std::shared_ptr<const Flibrary::IDatabaseController> databaseController,
                     std::shared_ptr<const Flibrary::IAuthorAnnotationController> authorAnnotationController,
                     std::shared_ptr<const ICoverCache> coverCache,
                     std::shared_ptr<const IBookExtractor> bookExtractor,
                     std::shared_ptr<const INoSqlRequester> noSqlRequester,
                     std::shared_ptr<Flibrary::IAnnotationController> annotationController)
	: m_impl(std::move(settings),
             std::move(collectionProvider),
             std::move(databaseController),
             std::move(authorAnnotationController),
             std::move(coverCache),
             std::move(bookExtractor),
             std::move(noSqlRequester),
             std::move(annotationController))
{
	PLOGV << "Requester created";
}

Requester::~Requester()
{
	PLOGV << "Requester destroyed";
}

QByteArray Requester::Search(const QString& root, const Parameters& parameters) const
{
	return GetImpl(*m_impl, &Impl::Search, ContentType::Books, std::cref(root), std::cref(parameters));
}

QByteArray Requester::GetBookText(const QString& root, const Parameters& parameters) const
{
	const auto bookId = GetParameter(parameters, "book");
	assert(!bookId.isEmpty());
	auto result = m_impl->GetBookText(bookId);
	return PostProcess(ContentType::BookText, root, *m_impl, result, { root, bookId });
}

#define OPDS_INVOKER_ITEM(NAME)                                                                                                                         \
	QByteArray Requester::Get##NAME(const QString& root, const Parameters& parameters) const                                                            \
	{                                                                                                                                                   \
		return GetImpl(*m_impl, &Impl::GetNavigation, ContentType::Navigation, std::cref(root), std::cref(parameters), Flibrary::NavigationMode::NAME); \
	}
OPDS_NAVIGATION_ITEMS_X_MACRO
#undef OPDS_INVOKER_ITEM

#define OPDS_INVOKER_ITEM(NAME)                                                                               \
	QByteArray Requester::Get##NAME(const QString& root, const Parameters& parameters) const                  \
	{                                                                                                         \
		return GetImpl(*m_impl, &Impl::Get##NAME, ContentType::NAME, std::cref(root), std::cref(parameters)); \
	}
OPDS_ADDITIONAL_ITEMS_X_MACRO
#undef OPDS_INVOKER_ITEM
