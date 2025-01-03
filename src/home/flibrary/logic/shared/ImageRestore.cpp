#include "ImageRestore.h"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>

#include <plog/Log.h>

#include "util/xml/SaxParser.h"
#include "util/xml/XmlAttributes.h"
#include "util/xml/XmlWriter.h"
#include "common/Constant.h"

#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

using Covers = std::unordered_map<QString, QByteArray>;

constexpr auto BINARY = "binary";
constexpr auto ID = "id";
constexpr auto CONTENT_TYPE = "content-type";
constexpr auto JPEG = "image/jpeg";

class SaxPrinter final
	: public Util::SaxParser
{
public:
	SaxPrinter(QIODevice & input, QIODevice & output, Covers covers)
		: SaxParser { input }
		, m_writer { output }
		, m_covers { std::move(covers) }
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

		if (name == BINARY)
			WriteImage(attributes.GetAttribute(ID));

		return true;
	}

	bool OnEndElement(const QString & name, const QString & path) override
	{
		if (path == "FictionBook")
			WriteImages();

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
				const auto it = m_covers.find(imageFileName);
				assert(it != m_covers.end());
				auto result = std::move(it->second);
				m_covers.erase(it);
				return result;
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

	void WriteImages()
	{
		for (const auto & [name, body] : m_covers)
		{
			m_writer
				.WriteStartElement(BINARY)
				.WriteAttribute(ID, name)
				.WriteAttribute(CONTENT_TYPE, JPEG)
				.WriteCharacters(QString::fromUtf8(body.toBase64()))
				.WriteEndElement(BINARY)
				;
		}

		m_covers.clear();
	}

private:
	Util::XmlWriter m_writer;
	Covers m_covers;
	bool m_hasError { false };
};

QByteArray RestoreImagesImpl(QIODevice & stream, Covers covers)
{
	QByteArray byteArray;
	QBuffer buffer(&byteArray);
	buffer.open(QIODevice::WriteOnly);
	const SaxPrinter saxPrinter(stream, buffer, std::move(covers));
	return saxPrinter.HasError() ? QByteArray {} : byteArray;
}

QByteArray RestoreImagesImpl(QIODevice & stream, const QString & folder, const QString & fileName)
{
	Covers covers;
	ExtractBookImages(folder, fileName, [&covers] (auto name, auto body)
	{
		covers.try_emplace(std::move(name), std::move(body));
		return false;
	});
	if (covers.empty())
		return stream.readAll();

	if (auto byteArray = RestoreImagesImpl(stream, std::move(covers)); !byteArray.isEmpty())
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
	stop = callback(Global::COVER, stream.readAll());
	return true;
}

void ParseImages(const QString & folder, const QString & fileName, const ExtractBookImagesCallback & callback)
{
	if (!QFile::exists(folder))
		return;

	const Zip zip(folder);
	auto fileList = zip.GetFileNameList();
	const auto filePrefix = QString("%1/").arg(QFileInfo(fileName).completeBaseName());
	if (const auto [begin, end] = std::ranges::remove_if(fileList, [&] (const auto & item)
	{
		return !item.startsWith(filePrefix) /*|| item == filePrefix*/;
	}); begin != end)
		fileList.erase(begin, end);

	for (const auto & file : fileList)
	{
		auto body = zip.Read(file).readAll();
		if (!body.isEmpty() && callback(file.split('/').back(), std::move(body)))
			return;
	}
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
		return ParseCovers(QString("%1/%2/%3.%4").arg(fileInfo.dir().path(), Global::COVERS, fileInfo.completeBaseName(), ext), fileName, callback, stop) || init;
	});

	for (const auto * ext : EXTENSIONS)
		ParseImages(QString("%1/%2/%3.%4").arg(fileInfo.dir().path(), Global::IMAGES, fileInfo.completeBaseName(), ext), fileName, callback);

	return result;
}

}
