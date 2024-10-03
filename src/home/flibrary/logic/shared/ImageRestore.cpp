#include "ImageRestore.h"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>

#include <plog/Log.h>

#include "util/xml/SaxParser.h"
#include "util/xml/XmlAttributes.h"
#include "util/xml/XmlWriter.h"

#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

class SaxPrinter final
	: public Util::SaxParser
{
public:
	SaxPrinter(QIODevice & input, QIODevice & output, Zip & zip, const QString & bookFileName)
		: SaxParser(input)
		, m_writer(output)
		, m_zip(zip)
		, m_bookFileName(bookFileName)
	{
		Parse();
	}

	bool HasError() const noexcept
	{
		return m_hasError;
	}

private: // Util::SaxParser
	bool OnProcessingInstruction(const QString & target, const QString & data) override
	{
		return m_writer.WriteProcessingInstruction(target, data), true;
	}

	bool OnStartElement(const QString & name, const QString & /*path*/, const Util::XmlAttributes & attributes) override
	{
		m_writer.WriteStartElement(name, attributes);

		if (name == "binary")
			WriteImage(attributes.GetAttribute("id"));

		return true;
	}

	bool OnEndElement(const QString & name, const QString & /*path*/) override
	{
		return m_writer.WriteEndElement(name), true;
	}

	bool OnCharacters(const QString & /*path*/, const QString & value) override
	{
		return m_writer.WriteCharacters(value), true;
	}

	bool OnWarning(const QString & text) override
	{
		PLOGW << text;
		return true;
	}

	bool OnError(const QString & text) override
	{
		m_hasError = true;
		PLOGE << text;
		return false;
	}

	bool OnFatalError(const QString & text) override
	{
		return OnError(text);
	}

private:
	void WriteImage(const QString & imageFileName)
	{
		const auto data = [&]
		{
			try
			{
				return m_zip.Read(QString("%1/%2").arg(m_bookFileName, imageFileName)).readAll();
			}
			catch (const std::exception & ex)
			{
				PLOGE << ex.what();
			}
			return QByteArray {};
		}();

		if (data.isEmpty())
		{
			PLOGW << imageFileName << " not found";
			return;
		}

		const auto image = QString::fromUtf8(data.toBase64());
		OnCharacters({}, image);
	}

private:
	Util::XmlWriter m_writer;
	Zip & m_zip;
	const QString & m_bookFileName;
	bool m_hasError { false };
};

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

QByteArray RestoreImagesImpl(QIODevice & stream, const QString & folder, const QString & fileName)
{
	const auto zip = CreateImageArchive(folder);
	if (!zip)
		return stream.readAll();

	QByteArray byteArray;
	QBuffer buffer(&byteArray);
	buffer.open(QIODevice::WriteOnly);
	const SaxPrinter saxPrinter(stream, buffer, *zip, fileName);
	buffer.close();
	if (!saxPrinter.HasError())
		return byteArray;

	stream.seek(0);
	return stream.readAll();
}

bool ParseCovers(const QString & folder, const QString & fileName, const ExtractBookImagesCallback & callback, bool & stop)
{
	if (!QFile::exists(folder))
		return false;

	const Zip zip(folder);
	const auto fileList = zip.GetFileNameList();
	const auto file = QFileInfo(fileName).completeBaseName();
	if (!fileList.contains(file))
		return false;

	auto & stream = zip.Read(file);
	stop = callback("cover", stream.readAll());
	return true;
}

void ParseImages(const QString & folder, const QString & fileName, const ExtractBookImagesCallback & callback)
{
	if (!QFile::exists(folder))
		return;

	const Zip zip(folder);
	auto fileList = zip.GetFileNameList();
	const auto filePrefix = QFileInfo(fileName).completeBaseName();
	if (const auto [begin, end] = std::ranges::remove_if(fileList, [&] (const auto & item)
	{
		return !item.startsWith(filePrefix);
	}); begin != end)
		fileList.erase(begin, end);

	for (const auto & file : fileList)
		if (callback(file.split('/').back(), zip.Read(file).readAll()))
			return;
}

}

namespace HomeCompa::Flibrary {

QByteArray RestoreImages(QIODevice & input, const QString & folder, const QString & fileName)
{
	return RestoreImagesImpl(input, folder, fileName);
}

bool ExtractBookImages(const QString & folder, const QString & fileName, const ExtractBookImagesCallback & callback)
{
	static constexpr const char * EXTENSIONS[] { "zip", "7z" };

	const QFileInfo fileInfo(folder);

	bool stop = false;
	const auto result = std::accumulate(std::cbegin(EXTENSIONS), std::cend(EXTENSIONS), false, [&] (const bool init, const char * ext)
	{
		return ParseCovers(QString("%1/%2_covers.%3").arg(fileInfo.dir().path(), fileInfo.completeBaseName(), ext), fileName, callback, stop) || init;
	});

	for (const auto * ext : EXTENSIONS)
		ParseImages(QString("%1/%2_images.%3").arg(fileInfo.dir().path(), fileInfo.completeBaseName(), ext), fileName, callback);

	return result;
}

}
