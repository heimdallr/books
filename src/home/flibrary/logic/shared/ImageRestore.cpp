#include "ImageRestore.h"

#include <QBuffer>
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
