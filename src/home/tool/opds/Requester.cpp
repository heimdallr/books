#include "Requester.h"

#include <ranges>

#include <QBuffer>
#include <QByteArray>
#include <QDateTime>

#include <plog/Log.h>

#include "fnd/ScopedCall.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "interface/constants/GenresLocalization.h"
#include "interface/constants/Localization.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseController.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/SortNavigation.h"
#include "Util/xml/XmlWriter.h"
#include "Util/localization.h"
#include "Util/timer.h"

using namespace HomeCompa;
using namespace Opds;

namespace {

constexpr auto AUTHOR = "a.LastName || coalesce(' ' || nullif(substr(a.FirstName, 1, 1), '') || '.' || coalesce(nullif(substr(a.middleName, 1, 1), '') || '.', ''), '')";
constexpr auto SERIES = "coalesce(' ' || s.SeriesTitle || coalesce(' #'||nullif(b.SeqNumber, 0), ''), '')";


constexpr auto CONTEXT = "Requester";
constexpr auto COUNT = QT_TRANSLATE_NOOP("Requester", "Number of: %1");

TR_DEF

struct Node
{
    using Attributes = std::unordered_map<QString, QString>;
    using Children = std::vector<Node>;
    QString name;
    QString value;
    Attributes attributes;
    Children children;

    QString title;
};

std::vector<Node> GetStandardNodes(QString id, QString title)
{
    return std::vector<Node>{
    	{ "updated", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ssZ") },
        { "id", std::move(id) },
        { "title", std::move(title) },
    };
}

Util::XmlWriter & operator<<(Util::XmlWriter & writer, const Node & node)
{
    ScopedCall nameGuard([&] { writer.WriteStartElement(node.name); }, [&] { writer.WriteEndElement(); });
    std::ranges::for_each(node.attributes, [&] (const auto & item) { writer.WriteAttribute(item.first, item.second); });
    writer.WriteCharacters(node.value);
    std::ranges::for_each(node.children, [&] (const auto & item) { writer << item; });
    return writer;
}

Node GetHead(QString id, QString title)
{
    auto standardNodes = GetStandardNodes(std::move(id), std::move(title));
    standardNodes.emplace_back("link", QString {}, Node::Attributes { {"href", "/opds"}, {"rel", "start"}, {"type", "application/atom+xml;profile=opds-catalog;kind=navigation"}});
//    standardNodes.emplace_back("link", QString {}, Node::Attributes { {"href", "/opds"}, {"rel", "self"}, {"type", "application/atom+xml;profile=opds-catalog;kind=navigation"} });
    return Node { "feed", {}, {
                { "xmlns", "http://www.w3.org/2005/Atom" },
                { "xmlns:dc", "http://purl.org/dc/terms/" },
                { "xmlns:opds", "http://opds-spec.org/2010/catalog" },
            }
            , std::move(standardNodes)
    };
}

Node & WriteEntry(Node::Children & children
    , QString id
    , QString title
    , const int count
    , QString content = {}
    , const bool isCatalog = true
)
{
    auto href = QString("/opds/%1").arg(id);
    auto & entry = children.emplace_back("entry", QString{}, Node::Attributes{}, GetStandardNodes(std::move(id), title));
    entry.title = std::move(title);
    if (isCatalog)
		entry.children.emplace_back("link", QString {}, Node::Attributes { {"href", std::move(href)}, {"rel", "subsection"}, {"type", "application/atom+xml;profile=opds-catalog;kind=navigation"} });
    entry.children.emplace_back("content", content.isEmpty() ? Tr(COUNT).arg(count) : std::move(content), Node::Attributes{{"type", "text"}});
    return entry;
}

std::vector<std::pair<QString, int>> ReadStartsWith(DB::IDatabase & db
    , const QString & queryText
    , const QString & value = {}
)
{
    std::vector<std::pair<QString, int>> result;

    const auto query = db.CreateQuery(queryText.toStdString());
    Util::Timer t(L"ReadStartsWith: " + value.toStdWString());
    query->Bind(0, value.toStdString());
    query->Bind(1, value.toStdString() + "%");

    for (query->Execute(); !query->Eof(); query->Next())
        result.emplace_back(query->Get<const char *>(0), query->Get<int>(1));

    std::ranges::sort(result, [] (const auto & lhs, const auto & rhs) { return QStringWrapper { lhs.first } < QStringWrapper { rhs.first }; });
    std::ranges::transform(result, result.begin(), [&] (const auto & item) { return std::make_pair(value + item.first, item.second); });

    return result;
}

void WriteNavigationEntries(DB::IDatabase & db
    , const char * navigationType
    , const std::string & queryText
    , const QString & arg
    , Node::Children & children
)
{
    const auto query = db.CreateQuery(queryText);
    Util::Timer t(L"WriteNavigationEntry: " + arg.toStdWString());
    query->Bind(0, arg.toStdString());
    for (query->Execute(); !query->Eof(); query->Next())
    {
        const auto id = query->Get<int>(0);
        auto entryId = QString("%1/%2").arg(navigationType).arg(id);
        auto href = QString("/opds/%1").arg(entryId);
        auto content = query->ColumnCount() > 3 ? query->Get<const char *>(3) : QString {};

        auto & entry = WriteEntry(children, std::move(entryId), query->Get<const char *>(1), query->Get<int>(2), std::move(content));
        entry.children.emplace_back("link", QString {}, Node::Attributes { {"href", std::move(href)}, {"rel", "subsection"}, {"type", "application/atom+xml;profile=opds-catalog;kind=navigation"} });
    }
};

void WriteBookEntries(DB::IDatabase & db
    , const char *
    , const std::string & queryText
    , const QString & arg
    , Node::Children & children
)
{
    const auto query = db.CreateQuery(queryText);
    Util::Timer t(L"WriteBookEntries: " + arg.toStdWString());
    query->Bind(0, arg.toStdString());
    for (query->Execute(); !query->Eof(); query->Next())
    {
        const auto id = query->Get<int>(0);
        auto entryId = QString("Book/%1").arg(id);
        auto href = QString("/opds/%1").arg(entryId);
        auto content = query->ColumnCount() > 3 ? query->Get<const char *>(3) : QString {};

        auto & entry = WriteEntry(children, std::move(entryId), query->Get<const char *>(1), query->Get<int>(2), std::move(content), false);
        entry.children.emplace_back("link", QString {}, Node::Attributes { {"href", std::move(href)}, {"rel", "subsection"}, {"type", "application/atom+xml;profile=opds-catalog;kind=acquisition"} });
    }
};

using EntryWriter = void(*)(DB::IDatabase & db
    , const char * navigationType
    , const std::string & queryText
    , const QString & arg
    , Node::Children & children);

Node WriteNavigationStartsWith(DB::IDatabase & db
    , const QString & value
    , const char * navigationType
    , const QString & startsWithQuery
    , const QString & itemQuery
    , const EntryWriter entryWriter
)
{
    auto head = GetHead(navigationType, Loc::Tr(Loc::NAVIGATION, navigationType));
    auto & children = head.children;

    std::vector<std::pair<QString, int>> dictionary { {value, 0} };
    do
    {
        decltype(dictionary) buffer;
        std::ranges::sort(dictionary, [] (const auto & lhs, const auto & rhs) { return lhs.second > rhs.second; });
        while (!dictionary.empty() && dictionary.size() + children.size() + buffer.size() < 20)
        {
            auto [ch, count] = std::move(dictionary.back());
            dictionary.pop_back();

            if (!ch.isEmpty())
                entryWriter(db, navigationType, itemQuery.arg("=").toStdString(), ch, children);

            auto startsWith = ReadStartsWith(db, startsWithQuery.arg(ch.length() + 1), ch);
            auto tail = std::ranges::partition(startsWith, [] (const auto & item) { return item.second > 1; });
            for (const auto & singleCh : tail | std::views::keys)
                entryWriter(db, navigationType, itemQuery.arg("like").toStdString(), singleCh + "%", children);

            startsWith.erase(std::begin(tail), std::end(startsWith));
            std::ranges::copy(startsWith, std::back_inserter(buffer));
        }

        std::ranges::copy(buffer, std::back_inserter(dictionary));
        buffer.clear();
    }
    while (!dictionary.empty() && dictionary.size() + children.size() < 20);

    for (const auto & [ch, count] : dictionary)
    {
        auto id = QString("%1/starts/%2").arg(navigationType, ch);
        id.replace(' ', "%20");
        auto title = ch;
        if (title.endsWith(' '))
            title[title.length() - 1] = QChar(0xB7);
        WriteEntry(children, id, QString("%1~").arg(title), count);
    }

	std::ranges::sort(children, [] (const auto & lhs, const auto & rhs) { return QStringWrapper(lhs.title) < QStringWrapper(rhs.title); });

    return head;
}

}

struct Requester::Impl
{
    std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider;
    std::shared_ptr<const Flibrary::IDatabaseController> databaseController;
    Impl(std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider
        , std::shared_ptr<const Flibrary::IDatabaseController> databaseController
    )
        : collectionProvider { std::move(collectionProvider) }
        , databaseController { std::move(databaseController) }
    {
    }

    Node WriteRoot() const
    {
        auto head = GetHead("root", collectionProvider->GetActiveCollection().name);

        const auto dbStatQueryTextItems = QStringList {}
#define     OPDS_ROOT_ITEM(NAME) << QString("select '%1', count(42) from %1").arg(Loc::NAME)
		    OPDS_ROOT_ITEMS_X_MACRO
#undef      OPDS_ROOT_ITEM
			;

        auto dbStatQueryText = dbStatQueryTextItems.join(" union all ");
        dbStatQueryText
    		.replace("count(42) from Archives", "count(distinct b.Folder) from Books b")
    		.replace("count(42) from Groups", "count(42) from Groups_User")
    		;

        const auto db = databaseController->GetDatabase(true);
        const auto query = db->CreateQuery(dbStatQueryText.toStdString());
        for (query->Execute(); !query->Eof(); query->Next())
        {
            const auto * id = query->Get<const char *>(0);
            if (const auto count = query->Get<int>(1))
                WriteEntry(head.children, id, Loc::Tr(Loc::NAVIGATION, id), count);
        }

        return head;
    }

    Node WriteAuthorsNavigation(const QString & value) const
    {
        const auto startsWithQuery = QString("select %1, count(42) from Authors a where a.SearchName != ? and a.SearchName like ? group by %1").arg("substr(a.SearchName, %1, 1)");
        const QString navigationItemQuery = "select a.AuthorID, a.LastName || ' ' || a.FirstName || ' ' || a.MiddleName, count(42) from Authors a join Author_List l on l.AuthorID = a.AuthorID where a.SearchName %1 ? group by a.AuthorID";
        return WriteNavigationStartsWith(*databaseController->GetDatabase(true), value, Loc::Authors, startsWithQuery, navigationItemQuery, &WriteNavigationEntries);
    }

    Node WriteSeriesNavigation(const QString & value) const
    {
        const auto startsWithQuery = QString("select %1, count(42) from Series a where a.SearchTitle != ? and a.SearchTitle like ? group by %1").arg("substr(a.SearchTitle, %1, 1)");
        const QString navigationItemQuery = "select a.SeriesID, a.SeriesTitle, count(42) from Series a join Books l on l.SeriesID = a.SeriesID where a.SearchTitle %1 ? group by a.SeriesID";
        return WriteNavigationStartsWith(*databaseController->GetDatabase(true), value, Loc::Series, startsWithQuery, navigationItemQuery, &WriteNavigationEntries);
    }

    void WriteGenresNavigationImpl(Node::Children & children, const QString & value) const
    {
        {
            const auto db = databaseController->GetDatabase(true);
            const auto query = db->CreateQuery("select g.GenreCode, g.FB2Code, (select count(42) from Genres l where l.ParentCode = g.GenreCode) + (select count(42) from Genre_List l where l.GenreCode = g.GenreCode) from Genres g where g.ParentCode = ? group by g.GenreCode");
            const auto arg = value.isEmpty() ? QString("0") : value;

            Util::Timer t(L"WriteGenresNavigationImpl: " + arg.toStdWString());
            query->Bind(0, arg.toStdString());
            for (query->Execute(); !query->Eof(); query->Next())
            {
                const auto id = QString("%1/%2").arg(Loc::Genres).arg(query->Get<const char *>(0));
                WriteEntry(children, id, Loc::Tr(Flibrary::GENRE, query->Get<const char *>(1)), query->Get<int>(2));
            }
        }
    }

    Node WriteGenresNavigation(const QString & value) const
    {
        auto head = GetHead(Loc::Genres, Loc::Tr(Loc::NAVIGATION, Loc::Genres));
        WriteGenresNavigationImpl(head.children, value);
        return head;
    }

    Node WriteKeywordsNavigation(const QString & value) const
    {
        const auto startsWithQuery = QString("select %1, count(42) from Keywords a where a.SearchTitle != ? and a.SearchTitle like ? group by %1").arg("substr(a.SearchTitle, %1, 1)");
        const QString navigationItemQuery = "select a.KeywordID, a.KeywordTitle, count(42) from Keywords a join Keyword_List l on l.KeywordID = a.KeywordID where a.SearchTitle %1 ? group by a.KeywordID";
        return WriteNavigationStartsWith(*databaseController->GetDatabase(true), value, Loc::Keywords, startsWithQuery, navigationItemQuery, &WriteNavigationEntries);
    }

    Node WriteArchivesNavigation(const QString & /*value*/) const
    {
        return {};
    }

    Node WriteGroupsNavigation(const QString & /*value*/) const
    {
        auto head = GetHead(Loc::Groups, Loc::Tr(Loc::NAVIGATION, Loc::Groups));

        const auto db = databaseController->GetDatabase(true);
        const auto query = db->CreateQuery("select g.GroupID, g.Title, count(42) from Groups_User g join Groups_List_User l on l.GroupID = g.GroupID group by g.GroupID");
        for (query->Execute(); !query->Eof(); query->Next())
        {
            const auto id = QString("%1/%2").arg(Loc::Groups).arg(query->Get<int>(0));
            WriteEntry(head.children, id, query->Get<const char *>(1), query->Get<int>(2));
        }

        return head;
    }

    Node WriteAuthorsBooks(const QString & navigationId, const QString & value) const
    {
        const auto startsWithQuery = QString("select substr(b.SearchTitle, %2, 1), count(42) from Books b join Author_List al on al.BookID = b.BookID join Authors a on a.AuthorID = al.AuthorID and a.AuthorID = %1 where b.SearchTitle != ? and b.SearchTitle like ? group by substr(b.SearchTitle, %2, 1)").arg(navigationId, "%1");
        const auto bookItemQuery = QString(R"(
            select b.BookID, b.Title || b.Ext, 0, %1 || %2
				from Books b 
            	join Author_List al on al.BookID = b.BookID
            	join Authors a on a.AuthorID = al.AuthorID and a.AuthorID = %3
				left join Series s on s.SeriesID = b.SeriesID
            	where b.SearchTitle %4 ?
            )").arg(AUTHOR, SERIES, navigationId, "%1");
        const auto navigationType = QString("%1/Books/%2").arg(Loc::Authors, navigationId).toStdString();
        return WriteNavigationStartsWith(*databaseController->GetDatabase(true), value, navigationType.data(), startsWithQuery, bookItemQuery, &WriteBookEntries);
    }

    Node WriteSeriesBooks(const QString & navigationId, const QString & value) const
    {
        const auto startsWithQuery = QString("select substr(b.SearchTitle, %2, 1), count(42) from Books b where b.SeriesID = %1 and b.SearchTitle != ? and b.SearchTitle like ? group by substr(b.SearchTitle, %2, 1)").arg(navigationId, "%1");
        const auto bookItemQuery = QString(R"(
                select b.BookID, b.Title || b.Ext, 0, (select %1 from Authors a join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID ORDER BY a.ROWID ASC LIMIT 1) || %2
                from Books b 
                left join Series s on s.SeriesID = b.SeriesID
                where b.SeriesID = %3 and b.SearchTitle %4 ?
            )").arg(AUTHOR, SERIES, navigationId, "%1");
        const auto navigationType = QString("%1/Books/%2").arg(Loc::Series, navigationId).toStdString();
        return WriteNavigationStartsWith(*databaseController->GetDatabase(true), value, navigationType.data(), startsWithQuery, bookItemQuery, &WriteBookEntries);
    }

    Node WriteGenresBooks(const QString & navigationId, const QString & value) const
    {
        const auto startsWithQuery = QString("select substr(b.SearchTitle, %2, 1), count(42) from Books b join Genre_List l on l.BookID = b.BookID and l.GenreCode = '%1' where b.SearchTitle != ? and b.SearchTitle like ? group by substr(b.SearchTitle, %2, 1)").arg(navigationId, "%1");
        const auto bookItemQuery = QString(R"(
                select b.BookID, b.Title || b.Ext, 0, (select %1 from Authors a join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID ORDER BY a.ROWID ASC LIMIT 1) || %2
                from Books b 
                join Genre_List l on l.BookID = b.BookID and l.GenreCode = '%3'
                left join Series s on s.SeriesID = b.SeriesID
                where b.SearchTitle %4 ?
            )").arg(AUTHOR, SERIES, navigationId, "%1");
        const auto navigationType = QString("%1/Books/%2").arg(Loc::Genres, navigationId).toStdString();
        auto node = WriteNavigationStartsWith(*databaseController->GetDatabase(true), value, navigationType.data(), startsWithQuery, bookItemQuery, &WriteBookEntries);
        WriteGenresNavigationImpl(node.children, navigationId);
        return node;
    }

    Node WriteKeywordsBooks(const QString & navigationId, const QString & value) const
    {
        const auto startsWithQuery = QString("select substr(b.SearchTitle, %2, 1), count(42) from Books b join Keyword_List l on l.BookID = b.BookID and l.KeywordID = %1 where b.SearchTitle != ? and b.SearchTitle like ? group by substr(b.SearchTitle, %2, 1)").arg(navigationId, "%1");
        const auto bookItemQuery = QString(R"(
                select b.BookID, b.Title || b.Ext, 0, (select %1 from Authors a join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID ORDER BY a.ROWID ASC LIMIT 1) || %2
                from Books b 
                join Keyword_List l on l.BookID = b.BookID and l.KeywordID = %3
                left join Series s on s.SeriesID = b.SeriesID
                where b.SearchTitle %4 ?
            )").arg(AUTHOR, SERIES, navigationId, "%1");
        const auto navigationType = QString("%1/Books/%2").arg(Loc::Keywords, navigationId).toStdString();
        return WriteNavigationStartsWith(*databaseController->GetDatabase(true), value, navigationType.data(), startsWithQuery, bookItemQuery, &WriteBookEntries);
    }

    Node WriteArchivesBooks(const QString & /*navigationId*/, const QString & /*value*/) const
    {
        return {};
    }

    Node WriteGroupsBooks(const QString & navigationId, const QString & value) const
    {
        const auto startsWithQuery = QString("select substr(b.SearchTitle, %2, 1), count(42) from Books b join Groups_List_User l on l.BookID = b.BookID and l.GroupID = %1 where b.SearchTitle != ? and b.SearchTitle like ? group by substr(b.SearchTitle, %2, 1)").arg(navigationId, "%1");
        const auto bookItemQuery = QString(R"(
                select b.BookID, b.Title || b.Ext, 0, (select %1 from Authors a join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID ORDER BY a.ROWID ASC LIMIT 1) || %2
                from Books b 
                join Groups_List_User l on l.BookID = b.BookID and l.GroupID = %3
                left join Series s on s.SeriesID = b.SeriesID
                where b.SearchTitle %4 ?
            )").arg(AUTHOR, SERIES, navigationId, "%1");
        const auto navigationType = QString("%1/Books/%2").arg(Loc::Groups, navigationId).toStdString();
        return WriteNavigationStartsWith(*databaseController->GetDatabase(true), value, navigationType.data(), startsWithQuery, bookItemQuery, &WriteBookEntries);
    }
};

Requester::Requester(std::shared_ptr<Flibrary::ICollectionProvider> collectionProvider
    , std::shared_ptr<Flibrary::IDatabaseController> databaseController
)
	: m_impl(std::move(collectionProvider)
        , std::move(databaseController)
    )
{
	PLOGD << "Requester created";
}

Requester::~Requester()
{
	PLOGD << "Requester destroyed";
}

QByteArray Requester::GetRoot() const
{
    return GetImpl(&Impl::WriteRoot);
}

#define OPDS_ROOT_ITEM(NAME) QByteArray Requester::Get##NAME##Navigation(const QString & value) const { return GetImpl(&Impl::Write##NAME##Navigation, value); }
		OPDS_ROOT_ITEMS_X_MACRO
#undef  OPDS_ROOT_ITEM

#define OPDS_ROOT_ITEM(NAME) QByteArray Requester::Get##NAME##Books(const QString & navigationId, const QString & value) const { return GetImpl(&Impl::Write##NAME##Books, navigationId, value); }
		OPDS_ROOT_ITEMS_X_MACRO
#undef  OPDS_ROOT_ITEM

template<typename NavigationGetter, typename... ARGS>
QByteArray Requester::GetImpl(NavigationGetter getter, const ARGS &... args) const
{
	if (!m_impl->collectionProvider->ActiveCollectionExists())
		return {};

	QBuffer buffer;
	try
    {
        const ScopedCall bufferGuard([&] { buffer.open(QIODevice::WriteOnly); }, [&] { buffer.close(); });
        const auto node = std::invoke(getter, *m_impl, std::cref(args)...);
        Util::XmlWriter writer(buffer);
        writer << node;
    }
    catch(const std::exception & ex)
    {
        PLOGE << ex.what();
        return {};
    }
    catch(...)
    {
        PLOGE << "Unknown error";
        return {};
    }

    PLOGD << buffer.buffer();

    return buffer.buffer();
}
