#include "Requester.h"

#include <QBuffer>
#include <QByteArray>
#include <QDateTime>

#include <plog/Log.h>

#include "fnd/ScopedCall.h"
#include "interface/constants/Localization.h"
#include "interface/logic/ICollectionProvider.h"
#include "Util/xml/XmlWriter.h"

using namespace HomeCompa;
using namespace Opds;

namespace {

void WriteStandardNode(Util::XmlWriter & writer, const QString & id, const QString & title)
{
    const auto now = QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ssZ");
    writer.WriteStartElement("updated").WriteCharacters(now).WriteEndElement();
    writer.WriteStartElement("id").WriteCharacters(id).WriteEndElement();
    writer.WriteStartElement("title").WriteCharacters(title).WriteEndElement();

}

void WriteEntry(Util::XmlWriter & writer, const char * id)
{
    const ScopedCall entryAuthors([&] { writer.WriteStartElement("entry"); }, [&] { writer.WriteEndElement(); });
    WriteStandardNode(writer, id, Loc::Tr(Loc::NAVIGATION, id));
    writer.WriteStartElement("link")
        .WriteAttribute("href", QString("/opds/%1").arg(id))
        .WriteAttribute("rel", "subsection")
        .WriteAttribute("type", "application/atom+xml;profile=opds-catalog;kind=navigation")
        .WriteEndElement();
}

void WriteRoot(QIODevice & output, const QString & collectionName)
{
    Util::XmlWriter writer(output);
    const ScopedCall feedGuard([&] { writer.WriteStartElement("feed"); }, [&] { writer.WriteEndElement(); });
    writer
        .WriteAttribute("xmlns", "http://www.w3.org/2005/Atom")
        .WriteAttribute("xmlns:dc", "http://purl.org/dc/terms/")
        .WriteAttribute("xmlns:opds", "http://opds-spec.org/2010/catalog")
        ;

    WriteStandardNode(writer, "root", collectionName);
    WriteEntry(writer, Loc::AUTHORS);
    WriteEntry(writer, Loc::GENRES);
    WriteEntry(writer, Loc::SERIES);
    WriteEntry(writer, Loc::KEYWORDS);
}

}

struct Requester::Impl
{
    std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider;
    Impl(std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider)
        : collectionProvider { std::move(collectionProvider) }
    {
    }
};

Requester::Requester(std::shared_ptr<Flibrary::ICollectionProvider> collectionProvider)
	: m_impl(std::move(collectionProvider))
{
	PLOGD << "Requester created";
}

Requester::~Requester()
{
	PLOGD << "Requester destroyed";
}

QByteArray Requester::GetRoot() const
{
	if (!m_impl->collectionProvider->ActiveCollectionExists())
		return {};

	QBuffer buffer;
	{
        const ScopedCall bufferGuard([&] { buffer.open(QIODevice::WriteOnly); }, [&] { buffer.close(); });
        WriteRoot(buffer, m_impl->collectionProvider->GetActiveCollection().name);
    }

    return buffer.buffer();
}
