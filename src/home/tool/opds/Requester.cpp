#include "Requester.h"

#include <ranges>

#include <QBuffer>
#include <QByteArray>
#include <QDateTime>

#include <plog/Log.h>

#include "fnd/ScopedCall.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
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

void WriteEntry(Util::XmlWriter & writer, const QString & id, const QString & title, const int count)
{
    const ScopedCall entryAuthors([&] { writer.WriteStartElement("entry"); }, [&] { writer.WriteEndElement(); });
    WriteStandardNode(writer, id, title);
    writer.WriteStartElement("link")
        .WriteAttribute("href", QString("/opds/%1").arg(id))
        .WriteAttribute("rel", "subsection")
        .WriteAttribute("type", "application/atom+xml;profile=opds-catalog;kind=navigation")
        .WriteEndElement();
    writer.WriteStartElement("content").WriteAttribute("type", "text").WriteCharacters(Tr(COUNT).arg(count)).WriteEndElement();
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
            WriteEntry(*writer, id, Loc::Tr(Loc::NAVIGATION, id), query->Get<int>(1));
        }
    }

    void WriteAuthors(QIODevice & output) const
    {
        const auto [writer, _] = WriteHead(output, Loc::Authors, Loc::Tr(Loc::NAVIGATION, Loc::Authors));
        const auto db = databaseController->GetDatabase(true);
        for (const auto & [ch, count] : ReadStartsWith(*db, "select MHL_UPPER(substr(a.LastName, 1, 1)), count(42) from Authors a group by MHL_UPPER(substr(a.LastName, 1, 1))"))
            WriteEntry(*writer, QString("%1/starts/%2").arg(Loc::Authors, ch), QString("%1~").arg(ch), count);
    }

    void WriteSeries(QIODevice & /*output*/) const
    {
    }

    void WriteGenres(QIODevice & /*output*/) const
    {
    }

    void WriteKeywords(QIODevice & /*output*/) const
    {
    }

    void WriteArchives(QIODevice & /*output*/) const
    {
    }

    void WriteGroups(QIODevice & output) const
    {
        const auto [writer, _] = WriteHead(output, Loc::Groups, Loc::Tr(Loc::NAVIGATION, Loc::Groups));
        const auto db = databaseController->GetDatabase(true);
        const auto query = db->CreateQuery("select g.GroupID, g.Title, count(42) from Groups_User g join Groups_List_User l on l.GroupID = g.GroupID group by g.GroupID, g.Title");
        for (query->Execute(); !query->Eof(); query->Next())
        {
            const auto id = QString("%1/%2").arg(Loc::Groups).arg(query->Get<int>(0));
            WriteEntry(*writer, id, query->Get<const char *>(1), query->Get<int>(2));
        }
    }

    void WriteAuthorsStartsWith(QIODevice & output, const QString & value) const
    {
        std::vector<Entry> entries;
        const auto [writer, _] = WriteHead(output, Loc::Authors, Loc::Tr(Loc::NAVIGATION, Loc::Authors));
        const ScopedCall resultGuard([&]
        {
            std::ranges::sort(entries, [] (const auto & lhs, const auto & rhs) { return QStringWrapper(lhs.title) < QStringWrapper(rhs.title); });
	        for (const auto & [id, title, count] : entries)
                WriteEntry(*writer, id, title, count);
        });

        const auto db = databaseController->GetDatabase(true);

        const auto writeAuthorEntries = [&] (const std::string & queryText, const QString & arg)
        {
            const auto query = db->CreateQuery(queryText);
            query->Bind(0, arg.toStdString());
            for (query->Execute(); !query->Eof(); query->Next())
            {
                const auto id = QString("%1/%2").arg(Loc::Authors).arg(query->Get<int>(0));
                const auto title = QString("%1 %2 %3").arg(query->Get<const char *>(1)).arg(query->Get<const char *>(2)).arg(query->Get<const char *>(3));
                entries.emplace_back(id, title, query->Get<int>(4));
            }
        };

        const auto query = QString("select %1, count(42) from Authors a where MHL_UPPER(a.LastName) != ? and MHL_UPPER(a.LastName) like ? group by %1").arg(QString("MHL_UPPER(substr(a.LastName, %1, 1))").arg("%1"));

        std::vector<std::pair<QString, int>> r { {value, 0} };
        do
        {
            decltype(r) nextR;
            std::ranges::sort(r, [] (const auto & lhs, const auto & rhs) { return lhs.second > rhs.second; });
            while (!r.empty() && r.size() + entries.size() + nextR.size() < 20)
            {
                auto [ch, count] = std::move(r.back());
                r.pop_back();

                writeAuthorEntries("select a.AuthorID, a.LastName, a.FirstName, a.MiddleName, count(42) from Authors a join Author_List l on l.AuthorID = a.AuthorID where MHL_UPPER(a.LastName) = ? group by a.AuthorID", ch);
                if (ch.isEmpty())
                    return;

                auto r1 = ReadStartsWith(*db, query.arg(ch.length() + 1), ch);
                auto tail = std::ranges::partition(r1, [] (const auto & item) { return item.second > 1; });
                for (const auto & ch1 : tail | std::views::keys)
                    writeAuthorEntries("select a.AuthorID, a.LastName, a.FirstName, a.MiddleName, count(42) from Authors a join Author_List l on l.AuthorID = a.AuthorID where MHL_UPPER(a.LastName) like ? group by a.AuthorID", ch1 + "%");

                r1.erase(std::begin(tail), std::end(r1));
                std::ranges::copy(r1, std::back_inserter(nextR));
            }

            std::ranges::copy(nextR, std::back_inserter(r));
            nextR.clear();
        }
        while (!r.empty() && r.size() + entries.size() < 20);

        for (const auto & [id, count] : r)
            entries.emplace_back(QString("%1/starts/%2").arg(Loc::Authors, id), QString("%1~").arg(id), count);
    }

    void WriteSeriesStartsWith(QIODevice & /*output*/, const QString & /*value*/) const
    {
    }

    void WriteGenresStartsWith(QIODevice & /*output*/, const QString & /*value*/) const
    {
        assert(false && "unexpected call");
    }

    void WriteKeywordsStartsWith(QIODevice & /*output*/, const QString & /*value*/) const
    {
    }

    void WriteArchivesStartsWith(QIODevice & /*output*/, const QString & /*value*/) const
    {
    }

    void WriteGroupsStartsWith(QIODevice & /*output*/, const QString & /*value*/) const
    {
        assert(false && "unexpected call");
    }

private:
    static std::vector<std::pair<QString, int>> ReadStartsWith(DB::IDatabase & db, const QString & queryText, const QString & value = {})
    {
        std::vector<std::pair<QString, int>> result;
        const auto query = db.CreateQuery(queryText.arg(value.length() + 1).toStdString());
        if (!value.isEmpty())
        {
            query->Bind(0, value.toStdString());
            query->Bind(1, value.toStdString() + "%");
        }

        for (query->Execute(); !query->Eof(); query->Next())
            result.emplace_back(query->Get<const char *>(0), query->Get<int>(1));

        std::ranges::sort(result, [] (const auto & lhs, const auto & rhs) { return QStringWrapper { lhs.first } < QStringWrapper { rhs.first }; });
        std::ranges::transform(result, result.begin(), [&] (const auto & item) { return std::make_pair(value + item.first, item.second); });

        return result;
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
    return GetBaseNavigation(&Impl::WriteRoot);
}

#define OPDS_ROOT_ITEM(NAME) QByteArray Requester::Get##NAME() const { return GetBaseNavigation(&Impl::Write##NAME); }
		OPDS_ROOT_ITEMS_X_MACRO
#undef  OPDS_ROOT_ITEM

#define OPDS_ROOT_ITEM(NAME) QByteArray Requester::Get##NAME##StartsWith(const QString & value) const { return GetBaseNavigation(&Impl::Write##NAME##StartsWith, value); }
		OPDS_ROOT_ITEMS_X_MACRO
#undef  OPDS_ROOT_ITEM

template<typename NavigationGetter, typename... ARGS>
QByteArray Requester::GetBaseNavigation(NavigationGetter getter, const ARGS &... args) const
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
