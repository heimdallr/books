#include "ImageRestore.h"

#include <QDomNode>
#include <QFileInfo>

#include <plog/Log.h>

#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

QByteArray RestoreImagesImpl(QIODevice & stream, const QString & folder, const QString & fileName)
{
	auto input = stream.readAll();
	QDomDocument doc;
	if (!doc.setContent(input))
		return input;

	const auto root = doc.documentElement();

	std::vector<QDomNode> nodes;

	const auto nodeList = root.elementsByTagName("binary");
	for (int i = 0, sz = nodeList.size(); i < sz; ++i)
		if (auto node = nodeList.item(i); node.nodeValue().isEmpty())
			nodes.push_back(std::move(node));

	if (nodes.empty())
		return input;

	const auto zip = CreateImageArchive(folder);
	if (!zip)
		return input;

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


	for (auto & node : nodes)
	{
		const auto bytes = read(node.attributes().namedItem("id").nodeValue());
		if (bytes.isEmpty())
			continue;

		auto image = QString::fromUtf8(bytes.toBase64());
		node.appendChild(doc.createTextNode(image));
	}

	doc.removeChild(doc.firstChild());
	doc.insertBefore(doc.createProcessingInstruction("xml", R"(version="1.0" encoding="utf-8")"), doc.firstChild());
	return doc.toByteArray();
}

}

namespace HomeCompa::Flibrary {

QByteArray RestoreImages(QIODevice & input, const QString & folder, const QString & fileName)
{
	return RestoreImagesImpl(input, folder, fileName);
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
