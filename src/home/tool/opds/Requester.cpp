#include "Requester.h"

#include <ranges>

#include <QBuffer>
#include <QByteArray>
#include <QDateTime>

#include <plog/Log.h>

#include "fnd/NonCopyMovable.h"
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

using namespace HomeCompa;
using namespace Opds;

namespace {

constexpr auto AUTHOR = "a.LastName || coalesce(' ' || nullif(substr(a.FirstName, 1, 1), '') || '.' || coalesce(nullif(substr(a.middleName, 1, 1), '') || '.', ''), '')";
constexpr auto SERIES = "coalesce(' ' || s.SeriesTitle || coalesce(' #'||nullif(b.SeqNumber, 0), ''), '')";


constexpr auto CONTEXT = "Requester";
constexpr auto COUNT = QT_TRANSLATE_NOOP("Requester", "Number of: %1");

TR_DEF

void WriteStandardNode(Util::XmlWriter & writer, const QString & id, const QString & title)
{
    const auto now = QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ssZ");
    writer.WriteStartElement("updated").WriteCharacters(now).WriteEndElement();
    writer.WriteStartElement("id").WriteCharacters(id).WriteEndElement();
    writer.WriteStartElement("title").WriteCharacters(title).WriteEndElement();
}

struct Head
{
    std::unique_ptr<Util::XmlWriter> writer;
    std::unique_ptr<ScopedCall> guard;
};

struct Entry
{
    QString id;
    QString title;
    int count;
    QString content;
};

Head WriteHead(QIODevice & output, const QString & id, const QString & title)
{
    auto writer = std::make_unique<Util::XmlWriter>(output);
    auto guard = std::make_unique<ScopedCall>([w = writer.get()] { w->WriteStartElement("feed"); }, [w = writer.get()] { w->WriteEndElement(); });

    writer->
        WriteAttribute("xmlns", "http://www.w3.org/2005/Atom").
        WriteAttribute("xmlns:dc", "http://purl.org/dc/terms/").
        WriteAttribute("xmlns:opds", "http://opds-spec.org/2010/catalog");
    WriteStandardNode(*writer, id, title);
    return Head { std::move(writer), std::move(guard) };
}

void WriteEntry(Util::XmlWriter & writer
    , const QString & id
    , const QString & title
    , const int count
    , const QString & content = {}
)
{
    const ScopedCall entryAuthors([&] { writer.WriteStartElement("entry"); }, [&] { writer.WriteEndElement(); });
    WriteStandardNode(writer, id, title);
    writer.WriteStartElement("link")
        .WriteAttribute("href", QString("/opds/%1").arg(id))
        .WriteAttribute("rel", "subsection")
        .WriteAttribute("type", "application/atom+xml;profile=opds-catalog;kind=navigation")
        .WriteEndElement();
    writer.WriteStartElement("content").WriteAttribute("type", "text").WriteCharacters(content.isEmpty() ? Tr(COUNT).arg(count) : content).WriteEndElement();
}

std::vector<std::pair<QString, int>> ReadStartsWith(DB::IDatabase & db
    , const QString & queryText
    , const QString & value = {}
)
{
    std::vector<std::pair<QString, int>> result;

    const auto query = db.CreateQuery(queryText.toStdString());
    query->Bind(0, value.toStdString());
    query->Bind(1, value.toStdString() + "%");

    for (query->Execute(); !query->Eof(); query->Next())
        result.emplace_back(query->Get<const char *>(0), query->Get<int>(1));

    std::ranges::sort(result, [] (const auto & lhs, const auto & rhs) { return QStringWrapper { lhs.first } < QStringWrapper { rhs.first }; });
    std::ranges::transform(result, result.begin(), [&] (const auto & item) { return std::make_pair(value + item.first, item.second); });

    return result;
}

Head WriteNavigationStartsWith(DB::IDatabase & db
    , QIODevice & output
    , const QString & value
    , const char * navigationType
    , const QString & startsWithQuery
    , const QString & navigationItemQuery
)
{
    std::vector<Entry> entries;
    auto head = WriteHead(output, navigationType, Loc::Tr(Loc::NAVIGATION, navigationType));
    const auto & [writer, _] = head;
    const ScopedCall resultGuard([&]
    {
        std::ranges::sort(entries, [] (const auto & lhs, const auto & rhs) { return QStringWrapper(lhs.title) < QStringWrapper(rhs.title); });
        for (const auto & [id, title, count, content] : entries)
            WriteEntry(*writer, id, title, count, content);
    });

    const auto writeNavigationEntry = [&] (const std::string & queryText, const QString & arg)
    {
        const auto query = db.CreateQuery(queryText);
        query->Bind(0, arg.toStdString());
        for (query->Execute(); !query->Eof(); query->Next())
        {
            const auto id = QString("%1/%2").arg(navigationType).arg(query->Get<int>(0));
            const auto content = query->ColumnCount() > 3 ? query->Get<const char *>(3) : QString {};
            entries.emplace_back(id, query->Get<const char *>(1), query->Get<int>(2), content);
        }
    };

    std::vector<std::pair<QString, int>> dictionary { {value, 0} };
    do
    {
        decltype(dictionary) buffer;
        std::ranges::sort(dictionary, [] (const auto & lhs, const auto & rhs) { return lhs.second > rhs.second; });
        while (!dictionary.empty() && dictionary.size() + entries.size() + buffer.size() < 20)
        {
            auto [ch, count] = std::move(dictionary.back());
            dictionary.pop_back();

            if (!ch.isEmpty())
				writeNavigationEntry(navigationItemQuery.arg("=").toStdString(), ch);

            auto startsWith = ReadStartsWith(db, startsWithQuery.arg(ch.length() + 1), ch);
            auto tail = std::ranges::partition(startsWith, [] (const auto & item) { return item.second > 1; });
            for (const auto & singleCh : tail | std::views::keys)
                writeNavigationEntry(navigationItemQuery.arg("like").toStdString(), singleCh + "%");

            startsWith.erase(std::begin(tail), std::end(startsWith));
            std::ranges::copy(startsWith, std::back_inserter(buffer));
        }

        std::ranges::copy(buffer, std::back_inserter(dictionary));
        buffer.clear();
    }
    while (!dictionary.empty() && dictionary.size() + entries.size() < 20);

    for (const auto & [ch, count] : dictionary)
    {
        auto id = QString("%1/starts/%2").arg(navigationType, ch);
        id.replace(' ', "%20");
        auto title = ch;
        if (title.endsWith(' '))
            title[title.length() - 1] = QChar(0xB7);
        entries.emplace_back(id, QString("%1~").arg(title), count);
    }

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

    void WriteRoot(QIODevice & output) const
    {
        const auto [writer, _] = WriteHead(output, "root", collectionProvider->GetActiveCollection().name);

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
				WriteEntry(*writer, id, Loc::Tr(Loc::NAVIGATION, id), count);
        }
    }

    void WriteAuthorsNavigation(QIODevice & output, const QString & value) const
    {
        const auto startsWithQuery = QString("select %1, count(42) from Authors a where a.SearchName != ? and a.SearchName like ? group by %1").arg("substr(a.SearchName, %1, 1)");
        const QString navigationItemQuery = "select a.AuthorID, a.LastName || ' ' || a.FirstName || ' ' || a.MiddleName, count(42) from Authors a join Author_List l on l.AuthorID = a.AuthorID where a.SearchName %1 ? group by a.AuthorID";
        WriteNavigationStartsWith(*databaseController->GetDatabase(true), output, value, Loc::Authors, startsWithQuery, navigationItemQuery);
    }

    void WriteSeriesNavigation(QIODevice & output, const QString & value) const
    {
        const auto startsWithQuery = QString("select %1, count(42) from Series a where a.SearchTitle != ? and a.SearchTitle like ? group by %1").arg("substr(a.SearchTitle, %1, 1)");
        const QString navigationItemQuery = "select a.SeriesID, a.SeriesTitle, count(42) from Series a join Books l on l.SeriesID = a.SeriesID where a.SearchTitle %1 ? group by a.SeriesID";
        WriteNavigationStartsWith(*databaseController->GetDatabase(true), output, value, Loc::Series, startsWithQuery, navigationItemQuery);
    }

    void WriteGenresNavigationImpl(Util::XmlWriter & writer, const QString & value) const
    {
        const auto db = databaseController->GetDatabase(true);
        const auto query = db->CreateQuery("select g.GenreCode, g.FB2Code, (select count(42) from Genres l where l.ParentCode = g.GenreCode) + (select count(42) from Genre_List l where l.GenreCode = g.GenreCode) from Genres g where g.ParentCode = ? group by g.GenreCode");
        query->Bind(0, value.isEmpty() ? std::string("0") : value.toStdString());
        for (query->Execute(); !query->Eof(); query->Next())
        {
            const auto id = QString("%1/%2").arg(Loc::Genres).arg(query->Get<const char *>(0));
            WriteEntry(writer, id, Loc::Tr(Flibrary::GENRE, query->Get<const char *>(1)), query->Get<int>(2));
        }
    }

    void WriteGenresNavigation(QIODevice & output, const QString & value) const
    {
        const auto [writer, _] = WriteHead(output, Loc::Genres, Loc::Tr(Loc::NAVIGATION, Loc::Genres));
        WriteGenresNavigationImpl(*writer, value);
    }

    void WriteKeywordsNavigation(QIODevice & output, const QString & value) const
    {
        const auto startsWithQuery = QString("select %1, count(42) from Keywords a where a.SearchTitle != ? and a.SearchTitle like ? group by %1").arg("substr(a.SearchTitle, %1, 1)");
        const QString navigationItemQuery = "select a.KeywordID, a.KeywordTitle, count(42) from Keywords a join Keyword_List l on l.KeywordID = a.KeywordID where a.SearchTitle %1 ? group by a.KeywordID";
        WriteNavigationStartsWith(*databaseController->GetDatabase(true), output, value, Loc::Keywords, startsWithQuery, navigationItemQuery);
    }

    void WriteArchivesNavigation(QIODevice & /*output*/, const QString & /*value*/) const
    {
    }

    void WriteGroupsNavigation(QIODevice & output, const QString & /*value*/) const
    {
        const auto [writer, _] = WriteHead(output, Loc::Groups, Loc::Tr(Loc::NAVIGATION, Loc::Groups));
        const auto db = databaseController->GetDatabase(true);
        const auto query = db->CreateQuery("select g.GroupID, g.Title, count(42) from Groups_User g join Groups_List_User l on l.GroupID = g.GroupID group by g.GroupID");
        for (query->Execute(); !query->Eof(); query->Next())
        {
            const auto id = QString("%1/%2").arg(Loc::Groups).arg(query->Get<int>(0));
            WriteEntry(*writer, id, query->Get<const char *>(1), query->Get<int>(2));
        }
    }

    void WriteAuthorsBooks(QIODevice & output, const QString & navigationId, const QString & value) const
    {
        const auto startsWithQuery = QString("select substr(b.SearchTitle, %2, 1), count(42) from Books b join Author_List al on al.BookID = b.BookID join Authors a on a.AuthorID = al.AuthorID and a.AuthorID = %1 where b.SearchTitle != ? and b.SearchTitle like ? group by substr(b.SearchTitle, %2, 1)").arg(navigationId, "%1");
        const auto navigationItemQuery = QString(R"(
            select b.BookID, b.Title || b.Ext, 0, %1 || %2
				from Books b 
            	join Author_List al on al.BookID = b.BookID
            	join Authors a on a.AuthorID = al.AuthorID and a.AuthorID = %3
				left join Series s on s.SeriesID = b.SeriesID
            	where b.SearchTitle %4 ?
            )").arg(AUTHOR, SERIES, navigationId, "%1");
        const auto navigationType = QString("%1/Books/%2").arg(Loc::Authors, navigationId).toStdString();
        WriteNavigationStartsWith(*databaseController->GetDatabase(true), output, value, navigationType.data(), startsWithQuery, navigationItemQuery);
    }

    void WriteSeriesBooks(QIODevice & output, const QString & navigationId, const QString & value) const
    {
        const auto startsWithQuery = QString("select substr(b.SearchTitle, %2, 1), count(42) from Books b where b.SeriesID = %1 and b.SearchTitle != ? and b.SearchTitle like ? group by substr(b.SearchTitle, %2, 1)").arg(navigationId, "%1");
        const auto navigationItemQuery = QString(R"(
                select b.BookID, b.Title || b.Ext, 0, (select %1 from Authors a join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID ORDER BY a.ROWID ASC LIMIT 1) || %2
                from Books b 
                left join Series s on s.SeriesID = b.SeriesID
                where b.SeriesID = %3 and b.SearchTitle %4 ?
            )").arg(AUTHOR, SERIES, navigationId, "%1");
        const auto navigationType = QString("%1/Books/%2").arg(Loc::Series, navigationId).toStdString();
        WriteNavigationStartsWith(*databaseController->GetDatabase(true), output, value, navigationType.data(), startsWithQuery, navigationItemQuery);
    }

    void WriteGenresBooks(QIODevice & output, const QString & navigationId, const QString & value) const
    {
        const auto startsWithQuery = QString("select substr(b.SearchTitle, %2, 1), count(42) from Books b join Genre_List l on l.BookID = b.BookID and l.GenreCode = '%1' where b.SearchTitle != ? and b.SearchTitle like ? group by substr(b.SearchTitle, %2, 1)").arg(navigationId, "%1");
        const auto navigationItemQuery = QString(R"(
                select b.BookID, b.Title || b.Ext, 0, (select %1 from Authors a join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID ORDER BY a.ROWID ASC LIMIT 1) || %2
                from Books b 
                join Genre_List l on l.BookID = b.BookID and l.GenreCode = '%3'
                left join Series s on s.SeriesID = b.SeriesID
                where b.SearchTitle %4 ?
            )").arg(AUTHOR, SERIES, navigationId, "%1");
        const auto navigationType = QString("%1/Books/%2").arg(Loc::Genres, navigationId).toStdString();
        const auto [writer, _] = WriteNavigationStartsWith(*databaseController->GetDatabase(true), output, value, navigationType.data(), startsWithQuery, navigationItemQuery);
        WriteGenresNavigationImpl(*writer, navigationId);
    }

    void WriteKeywordsBooks(QIODevice & output, const QString & navigationId, const QString & value) const
    {
        const auto startsWithQuery = QString("select substr(b.SearchTitle, %2, 1), count(42) from Books b join Keyword_List l on l.BookID = b.BookID and l.KeywordID = %1 where b.SearchTitle != ? and b.SearchTitle like ? group by substr(b.SearchTitle, %2, 1)").arg(navigationId, "%1");
        const auto navigationItemQuery = QString(R"(
                select b.BookID, b.Title || b.Ext, 0, (select %1 from Authors a join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID ORDER BY a.ROWID ASC LIMIT 1) || %2
                from Books b 
                join Keyword_List l on l.BookID = b.BookID and l.KeywordID = %3
                left join Series s on s.SeriesID = b.SeriesID
                where b.SearchTitle %4 ?
            )").arg(AUTHOR, SERIES, navigationId, "%1");
        const auto navigationType = QString("%1/Books/%2").arg(Loc::Keywords, navigationId).toStdString();
        WriteNavigationStartsWith(*databaseController->GetDatabase(true), output, value, navigationType.data(), startsWithQuery, navigationItemQuery);
    }

    void WriteArchivesBooks(QIODevice & /*output*/, const QString & /*navigationId*/, const QString & /*value*/) const
    {
    }

    void WriteGroupsBooks(QIODevice & output, const QString & navigationId, const QString & value) const
    {
        const auto startsWithQuery = QString("select substr(b.SearchTitle, %2, 1), count(42) from Books b join Groups_List_User l on l.BookID = b.BookID and l.GroupID = %1 where b.SearchTitle != ? and b.SearchTitle like ? group by substr(b.SearchTitle, %2, 1)").arg(navigationId, "%1");
        const auto navigationItemQuery = QString(R"(
                select b.BookID, b.Title || b.Ext, 0, (select %1 from Authors a join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID ORDER BY a.ROWID ASC LIMIT 1) || %2
                from Books b 
                join Groups_List_User l on l.BookID = b.BookID and l.GroupID = %3
                left join Series s on s.SeriesID = b.SeriesID
                where b.SearchTitle %4 ?
            )").arg(AUTHOR, SERIES, navigationId, "%1");
        const auto navigationType = QString("%1/Books/%2").arg(Loc::Groups, navigationId).toStdString();
        WriteNavigationStartsWith(*databaseController->GetDatabase(true), output, value, navigationType.data(), startsWithQuery, navigationItemQuery);
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
        std::invoke(getter, *m_impl, std::ref(buffer), std::cref(args)...);
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
