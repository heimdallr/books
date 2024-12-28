#include "Requester.h"

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

std::unique_ptr<Util::XmlWriter> WriteHead(QIODevice & output, const QString & id, const QString & title)
{
    auto writer = std::make_unique<Util::XmlWriter>(output);
    const ScopedCall feedGuard([&] { writer->WriteStartElement("feed"); }, [&] { writer->WriteEndElement(); });
    writer->
        WriteAttribute("xmlns", "http://www.w3.org/2005/Atom").
        WriteAttribute("xmlns:dc", "http://purl.org/dc/terms/").
        WriteAttribute("xmlns:opds", "http://opds-spec.org/2010/catalog");
    WriteStandardNode(*writer, id, title);
    return writer;
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
        const auto writer = WriteHead(output, "root", collectionProvider->GetActiveCollection().name);

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
        const auto writer = WriteHead(output, Loc::Authors, Loc::Tr(Loc::NAVIGATION, Loc::Authors));
        const auto db = databaseController->GetDatabase(true);
        const auto query = db->CreateQuery("select MHL_UPPER(substr(a.LastName, 1, 1)), count(42) from Authors a group by MHL_UPPER(substr(a.LastName, 1, 1))");

        std::vector<std::pair<QString, int>> result;
        for (query->Execute(); !query->Eof(); query->Next())
            result.emplace_back(query->Get<const char *>(0), query->Get<int>(1));

        std::ranges::sort(result, [] (const auto & lhs, const auto & rhs) { return QStringWrapper { lhs.first } < QStringWrapper { rhs.first }; });
        for (const auto & [id, count] : result)
            WriteEntry(*writer, QString("%1/starts/%2").arg(Loc::Authors).arg(id), QString("%1~").arg(id), count);
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
        const auto writer = WriteHead(output, Loc::Groups, Loc::Tr(Loc::NAVIGATION, Loc::Groups));
        const auto db = databaseController->GetDatabase(true);
        const auto query = db->CreateQuery("select g.GroupID, g.Title, count(42) from Groups_User g join Groups_List_User l on l.GroupID = g.GroupID group by g.GroupID, g.Title");
        for (query->Execute(); !query->Eof(); query->Next())
        {
            const auto id = QString("%1/%2").arg(Loc::Groups).arg(query->Get<int>(0));
            WriteEntry(*writer, id, query->Get<const char *>(1), query->Get<int>(2));
        }
    }

    void WriteAuthorsStartsWith(QIODevice & /*output*/, const QString & /*value*/) const
    {
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
