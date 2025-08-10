#include "ImageRestore.h"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>

#include "interface/constants/SettingsConstant.h"

#include "common/Constant.h"
#include "jxl/jxl.h"
#include "util/ISettings.h"
#include "util/xml/SaxParser.h"
#include "util/xml/XmlAttributes.h"
#include "util/xml/XmlWriter.h"

#include "log.h"
#include "zip.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

using Covers = std::unordered_map<QString, QByteArray>;

constexpr auto BINARY = "binary";
constexpr auto ID = "id";
constexpr auto CONTENT_TYPE = "content-type";
constexpr auto JPEG = "jpeg";
constexpr auto PNG = "png";
constexpr auto JPEG_XL = "jpeg xl";

constexpr auto FICTION_BOOK = "FictionBook";
constexpr auto DESCRIPTION = "FictionBook/description";
constexpr auto DOCUMENT_INFO = "FictionBook/description/document-info";
constexpr auto PROGRAM_USED = "FictionBook/description/document-info/program-used";

using Decoder = QPixmap (*)(const QByteArray&);
using Recoder = std::pair<QByteArray, const char*> (*)(const QByteArray& bytes, const char* type);

QPixmap QtDecoder(const QByteArray& data)
{
	if (QPixmap pixmap; pixmap.loadFromData(data))
		return pixmap;

	return {};
}

QPixmap JxlDecoder(const QByteArray& data)
{
	auto image = JXL::Decode(data);
	return QPixmap::fromImage(std::move(image));
}

std::pair<QByteArray, const char*> QtRecoder(const QByteArray& data, const char* type)
{
	return std::make_pair(data, type);
}

std::pair<QByteArray, const char*> JxlRecoder(const QByteArray& data, const char*)
{
	std::pair<QByteArray, const char*> result;
	auto image = JXL::Decode(data);
	QByteArray bytes;
	{
		QBuffer buffer(&bytes);
		buffer.open(QIODevice::WriteOnly);
		const auto hasAlpha = image.pixelFormat().alphaUsage() == QPixelFormat::UsesAlpha;
		image.save(&buffer, hasAlpha ? PNG : JPEG);
		result.second = hasAlpha ? IMAGE_PNG : IMAGE_JPEG;
	}
	result.first = std::move(bytes);
	return result;
}

struct ImageFormatDescription
{
	const char* mediaType;
	const char* format;
	Decoder decoder;
	Recoder recoder;
};

constexpr ImageFormatDescription DEFAULT_DESCRIPTION { IMAGE_JPEG, JPEG, &QtDecoder, &QtRecoder };

constexpr std::pair<const char*, ImageFormatDescription> SIGNATURES[] {
	{ "\xFF\xD8\xFF\xE0",   { IMAGE_JPEG, JPEG, &QtDecoder, &QtRecoder } },
	{ "\x89\x50\x4E\x47",     { IMAGE_PNG, PNG, &QtDecoder, &QtRecoder } },
	{		 "\xFF\x0A", { nullptr, JPEG_XL, &JxlDecoder, &JxlRecoder } },
};

class SaxPrinter final : public Util::SaxParser
{
public:
	SaxPrinter(QIODevice& input, QIODevice& output, Covers covers)
		: SaxParser { input }
		, m_writer { output }
		, m_covers { std::move(covers) }
	{
		Parse();
		assert(m_hasProgramUsed);
	}

	bool HasError() const noexcept
	{
		return m_hasError;
	}

private: // Util::SaxParser
	bool OnProcessingInstruction(const QString& target, const QString& data) override
	{
		return m_writer.WriteProcessingInstruction(target, data), true;
	}

	bool OnStartElement(const QString& name, const QString& /*path*/, const Util::XmlAttributes& attributes) override
	{
		m_writer.WriteStartElement(name, attributes);

		if (name == BINARY)
			WriteImage(attributes.GetAttribute(ID));

		return true;
	}

	bool OnEndElement(const QString&, const QString& path) override
	{
		if (path == DOCUMENT_INFO && !m_hasProgramUsed)
		{
			m_writer.WriteStartElement("program-used").WriteCharacters(QString("%1 %2").arg(PRODUCT_ID, PRODUCT_VERSION)).WriteEndElement();
			m_hasProgramUsed = true;
		}

		if (path == DESCRIPTION && !m_hasProgramUsed)
		{
			m_writer.WriteStartElement("document-info").WriteStartElement("program-used").WriteCharacters(QString("%1 %2").arg(PRODUCT_ID, PRODUCT_VERSION)).WriteEndElement().WriteEndElement();
			m_hasProgramUsed = true;
		}

		if (path == FICTION_BOOK)
			WriteImages();

		return m_writer.WriteEndElement(), true;
	}

	bool OnCharacters(const QString& path, const QString& value) override
	{
		if (path != PROGRAM_USED)
			return m_writer.WriteCharacters(value), true;

		m_hasProgramUsed = true;
		return m_writer.WriteCharacters(QString("%1, %2 %3").arg(value, PRODUCT_ID, PRODUCT_VERSION)), true;
	}

	bool OnWarning(const QString& text) override
	{
		PLOGW << text;
		return true;
	}

	bool OnError(const QString& text) override
	{
		m_hasError = true;
		PLOGE << text;
		return false;
	}

	bool OnFatalError(const QString& text) override
	{
		return OnError(text);
	}

private:
	void WriteImage(const QString& imageFileName)
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
			catch (const std::exception& ex)
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
		for (const auto& [name, body] : m_covers)
		{
			const auto [recoded, mediaType] = Recode(body);
			m_writer.WriteStartElement(BINARY).WriteAttribute(ID, name).WriteAttribute(CONTENT_TYPE, mediaType).WriteCharacters(QString::fromUtf8(recoded.toBase64())).WriteEndElement();
		}

		m_covers.clear();
	}

private:
	Util::XmlWriter m_writer;
	Covers m_covers;
	bool m_hasError { false };
	bool m_hasProgramUsed { false };
};

QByteArray RestoreImagesImpl(QIODevice& stream, Covers covers)
{
	QByteArray byteArray;
	QBuffer buffer(&byteArray);
	buffer.open(QIODevice::WriteOnly);
	const SaxPrinter saxPrinter(stream, buffer, std::move(covers));
	return saxPrinter.HasError() ? QByteArray {} : byteArray;
}

void ConvertToGrayscale(QByteArray& srcImageBody, const int quality)
{
	const auto it = std::ranges::find_if(SIGNATURES, [&](const auto& item) { return srcImageBody.startsWith(item.first); });
	const auto* format = it != std::end(SIGNATURES) ? it->second.format : nullptr;

	QPixmap pixmap;
	if (!pixmap.loadFromData(srcImageBody, format))
		return;

	auto image = pixmap.toImage();
	image.convertTo(QImage::Format::Format_Grayscale8);

	QByteArray result;
	{
		QBuffer imageOutput(&result);
		if (!image.save(&imageOutput, format ? format : JPEG, quality))
			return;
	}

	srcImageBody = std::move(result);
}

QByteArray RestoreImagesImpl(QIODevice& stream, const QString& folder, const QString& fileName, const std::shared_ptr<const ISettings>& settings)
{
	Covers covers;
	ExtractBookImages(
		folder,
		fileName,
		[&covers](auto name, auto body)
		{
			covers.try_emplace(std::move(name), std::move(body));
			return false;
		},
		settings);
	if (covers.empty())
		return stream.readAll();

	if (auto byteArray = RestoreImagesImpl(stream, std::move(covers)); !byteArray.isEmpty())
		return byteArray;

	stream.seek(0);
	return stream.readAll();
}

bool ParseCover(const QString& folder, const QString& fileName, const ExtractBookImagesCallback& callback, bool& stop, const bool grayscale)
{
	if (!QFile::exists(folder))
		return false;

	const Zip zip(folder);
	const auto fileList = zip.GetFileNameList();
	const auto file = QFileInfo(fileName).completeBaseName();
	if (!fileList.contains(file))
		return false;

	const auto stream = zip.Read(file);
	auto body = stream->GetStream().readAll();
	if (grayscale)
		ConvertToGrayscale(body, 50);
	stop = callback(Global::COVER, std::move(body));
	return true;
}

void ParseImages(const QString& folder, const QString& fileName, const ExtractBookImagesCallback& callback, const bool grayscale)
{
	if (!QFile::exists(folder))
		return;

	const Zip zip(folder);
	auto fileList = zip.GetFileNameList();
	const auto filePrefix = QString("%1/").arg(QFileInfo(fileName).completeBaseName());
	if (const auto [begin, end] = std::ranges::remove_if(fileList, [&](const auto& item) { return !item.startsWith(filePrefix) /*|| item == filePrefix*/; }); begin != end)
		fileList.erase(begin, end);

	for (const auto& file : fileList)
	{
		auto body = zip.Read(file)->GetStream().readAll();
		if (grayscale)
			ConvertToGrayscale(body, 25);
		if (!body.isEmpty() && callback(file.split('/').back(), std::move(body)))
			return;
	}
}

} // namespace

namespace HomeCompa::Flibrary
{

QPixmap Decode(const QByteArray& bytes)
{
	assert(!bytes.isEmpty());
	const auto it = std::ranges::find_if(SIGNATURES, [&](const auto& item) { return bytes.startsWith(item.first); });
	const auto decoder = it != std::end(SIGNATURES) ? it->second.decoder : &QtDecoder;
	return decoder(bytes);
}

std::pair<QByteArray, const char*> Recode(const QByteArray& bytes)
{
	assert(!bytes.isEmpty());
	const auto it = std::ranges::find_if(SIGNATURES, [&](const auto& item) { return bytes.startsWith(item.first); });
	const auto& description = it != std::end(SIGNATURES) ? it->second : DEFAULT_DESCRIPTION;
	return description.recoder(bytes, description.mediaType);
}

QByteArray RestoreImages(QIODevice& input, const QString& folder, const QString& fileName, const std::shared_ptr<const ISettings>& settings)
{
	return RestoreImagesImpl(input, folder, fileName, settings);
}

QImage HasAlpha(const QImage& image, const char* data)
{
	if (memcmp(data, "\xFF\xD8\xFF\xE0", 4) == 0)
		return image.convertToFormat(QImage::Format_RGB888);

	if (!image.hasAlphaChannel())
		return image.convertToFormat(QImage::Format_RGB888);

	auto argb = image.convertToFormat(QImage::Format_RGBA8888);
	for (int i = 0, h = argb.height(), w = argb.width(); i < h; ++i)
	{
		const auto* pixels = reinterpret_cast<const QRgb*>(argb.constScanLine(i));
		if (std::any_of(pixels, pixels + static_cast<size_t>(w), [](const QRgb pixel) { return qAlpha(pixel) < UCHAR_MAX; }))
		{
			return argb;
		}
	}

	return image.convertToFormat(QImage::Format_RGB888);
}

void ExtractBookImages(const QString& folder, const QString& fileName, const ExtractBookImagesCallback& callback, const std::shared_ptr<const ISettings>& settings)
{
	static constexpr const char* EXTENSIONS[] { "zip", "7z" };

	const QFileInfo fileInfo(folder);

	bool stop = false;
	for (const auto* ext : EXTENSIONS)
	{
		try
		{
			ParseCover(QString("%1/%2/%3.%4").arg(fileInfo.dir().path(), Global::COVERS, fileInfo.completeBaseName(), ext),
			           fileName,
			           callback,
			           stop,
			           settings && settings->Get(Constant::Settings::EXPORT_GRAYSCALE_COVER_KEY, false));
		}
		catch (const std::exception& ex)
		{
			PLOGE << ex.what();
		}
		catch (...)
		{
			PLOGE << "unknown error";
		}
	}

	for (const auto* ext : EXTENSIONS)
	{
		try
		{
			ParseImages(QString("%1/%2/%3.%4").arg(fileInfo.dir().path(), Global::IMAGES, fileInfo.completeBaseName(), ext),
			            fileName,
			            callback,
			            settings && settings->Get(Constant::Settings::EXPORT_GRAYSCALE_IMAGES_KEY, false));
		}
		catch (const std::exception& ex)
		{
			PLOGE << ex.what();
		}
		catch (...)
		{
			PLOGE << "unknown error";
		}
	}
}

} // namespace HomeCompa::Flibrary
