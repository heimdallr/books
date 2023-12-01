#include "ImageRestore.h"

#include <QDomNode>
#include <QFileInfo>

#include <plog/Log.h>

#include "zip.h"

namespace HomeCompa::Flibrary {

QByteArray RestoreImages(const QByteArray & input, const QString & folder, const QString & fileName)
{
	QDomDocument doc;
	QString errorMsg;
	if (!doc.setContent(input, &errorMsg))
		return {};

	const auto root = doc.documentElement();

	std::vector<QDomNode> nodes;

	const auto nodeList = root.elementsByTagName("binary");
	for (int i = 0, sz = nodeList.size(); i < sz; ++i)
		if (auto node = nodeList.item(i); node.nodeValue().isEmpty())
			nodes.push_back(std::move(node));

	const auto zip = CreateImageArchive(folder);
	if (!zip)
		return doc.toByteArray();

	const auto read = [&] (const QString & id)
	{
		try
		{
			return zip->Read(QString("%1/%2").arg(fileName, id)).readAll();
		}
		catch (const std::exception & ex)
		{
			PLOGE << ex.what();
		}
		return QByteArray {};
	};


	for (auto& node : nodes)
	{
		const auto bytes = read(node.attributes().namedItem("id").nodeValue());
		if (bytes.isEmpty())
			continue;

		auto image = QString::fromUtf8(bytes.toBase64());
		node.appendChild(doc.createTextNode(image));
	}

	return doc.toByteArray();
}

std::unique_ptr<Zip> CreateImageArchive(const QString & folder)
{
	const QFileInfo info(folder);
	const auto basePath = QString("%1/%2.img").arg(info.absolutePath(), info.completeBaseName());
	for (const auto * ext : { ".zip", ".7z" })
	{
		const auto fileName = basePath + ext;
		if (!QFile::exists(fileName))
			continue;

		try
		{
			return std::make_unique<Zip>(fileName);
		}
		catch (const std::exception & ex)
		{
			PLOGE << ex.what();
		}
	}
	return {};
}

}
