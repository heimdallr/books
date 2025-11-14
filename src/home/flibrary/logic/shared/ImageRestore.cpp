#include "ImageRestore.h"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>

#include "fnd/try.h"

#include "interface/constants/SettingsConstant.h"

#include "util/ISettings.h"
#include "util/ImageUtil.h"
#include "util/xml/SaxParser.h"
#include "util/xml/XmlAttributes.h"
#include "util/xml/XmlWriter.h"

#include "Constant.h"
#include "zip.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

using Covers = std::unordered_map<QString, QByteArray>;

class SaxPrinter final : public Util::SaxParser
{
	static constexpr auto BINARY       = "binary";
	static constexpr auto ID           = "id";
	static constexpr auto CONTENT_TYPE = "content-type";

	static constexpr auto FICTION_BOOK       = "FictionBook";
	static constexpr auto DESCRIPTION        = "FictionBook/description";
	static constexpr auto DOCUMENT_INFO      = "FictionBook/description/document-info";
	static constexpr auto PROGRAM_USED       = "FictionBook/description/document-info/program-used";
	static constexpr auto AUTHOR             = "FictionBook/description/title-info/author";
	static constexpr auto AUTHOR_FIRST_NAME  = "first-name";
	static constexpr auto AUTHOR_LAST_NAME   = "last-name";
	static constexpr auto AUTHOR_MIDDLE_NAME = "middle-name";
	static constexpr auto BOOK_TITLE         = "FictionBook/description/title-info/book-title";
	static constexpr auto SEQUENCE           = "FictionBook/description/title-info/sequence";

	enum class ParseMode
	{
		Common = -1,
		Author,
		Title,
		Series,
	};

	static constexpr std::pair<const char*, ParseMode> PARSE_MODES[] {
		{     AUTHOR, ParseMode::Author },
		{ BOOK_TITLE,  ParseMode::Title },
		{   SEQUENCE, ParseMode::Series },
	};

public:
	SaxPrinter(QIODevice& input, QIODevice& output, Covers covers, std::unique_ptr<const ILogicFactory::ExtractedBook> metadataReplacement)
		: SaxParser { input }
		, m_writer { output }
		, m_covers { std::move(covers) }
		, m_metadataReplacement { std::move(metadataReplacement) }
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

	bool OnStartElement(const QString& name, const QString& path, const Util::XmlAttributes& attributes) override
	{
		if (m_currentMode != ParseMode::Common)
			return true;

		if (m_currentMode == ParseMode::Common && m_metadataReplacement)
		{
			m_currentMode = FindSecond(PARSE_MODES, path.toStdString().data(), ParseMode::Common, PszComparer {});
			if (m_currentMode != ParseMode::Common)
			{
				m_replacers.erase(m_currentMode);
				switch (m_currentMode) // NOLINT(clang-diagnostic-switch-enum)
				{
					case ParseMode::Author:
						return WriteAuthor(), true;

					case ParseMode::Title:
						return WriteTitle(), true;

					case ParseMode::Series:
						return WriteSeries(), true;

					default:
						assert(false && "unexpected mode");
						break;
				}
			}
		}

		m_writer.WriteStartElement(name, attributes);

		if (name == BINARY && !m_covers.empty())
			WriteImage(attributes.GetAttribute(ID));

		return true;
	}

	bool OnEndElement(const QString&, const QString& path) override
	{
		if (m_currentMode != ParseMode::Common)
		{
			if (path == PARSE_MODES[static_cast<size_t>(m_currentMode)].first)
				m_currentMode = ParseMode::Common;
			return true;
		}

		if (m_currentMode != ParseMode::Common && path == PARSE_MODES[static_cast<size_t>(m_currentMode)].first)
			return m_currentMode = ParseMode::Common, true;

		if (path == DOCUMENT_INFO && !m_hasProgramUsed)
		{
			m_writer.WriteStartElement("program-used").WriteCharacters(QString("%1 %2").arg(PRODUCT_ID, PRODUCT_VERSION)).WriteEndElement();
			m_hasProgramUsed = true;
		}

		if (path == DESCRIPTION)
		{
			if (!m_hasProgramUsed)
			{
				m_writer.WriteStartElement("document-info").WriteStartElement("program-used").WriteCharacters(QString("%1 %2").arg(PRODUCT_ID, PRODUCT_VERSION)).WriteEndElement().WriteEndElement();
				m_hasProgramUsed = true;
			}

			if (m_metadataReplacement)
				for (const auto invoker : m_replacers | std::views::values)
					std::invoke(invoker, this);
		}

		if (path == FICTION_BOOK)
			WriteImages();

		return m_writer.WriteEndElement(), true;
	}

	bool OnCharacters(const QString& path, const QString& value) override
	{
		if (m_currentMode != ParseMode::Common)
			return true;

		if (path != PROGRAM_USED)
			return m_writer.WriteCharacters(value), true;

		m_hasProgramUsed = true;
		return m_writer.WriteCharacters(QString("%1, %2 %3").arg(value, PRODUCT_ID, PRODUCT_VERSION)), true;
	}

	bool OnWarning(const size_t line, const size_t column, const QString& text) override
	{
		PLOGW << line << ":" << column << " " << text;
		return true;
	}

	bool OnError(const size_t line, const size_t column, const QString& text) override
	{
		m_hasError = true;
		PLOGE << line << ":" << column << " " << text;
		return false;
	}

	bool OnFatalError(const size_t line, const size_t column, const QString& text) override
	{
		return OnError(line, column, text);
	}

private:
	void WriteAuthor()
	{
		assert(m_metadataReplacement);
		const auto node = m_writer.Guard("author");
		node->WriteStartElement(AUTHOR_FIRST_NAME).WriteCharacters(m_metadataReplacement->authorFull.firstName).WriteEndElement();
		node->WriteStartElement(AUTHOR_MIDDLE_NAME).WriteCharacters(m_metadataReplacement->authorFull.middleName).WriteEndElement();
		node->WriteStartElement(AUTHOR_LAST_NAME).WriteCharacters(m_metadataReplacement->authorFull.lastName).WriteEndElement();
	}

	void WriteTitle()
	{
		assert(m_metadataReplacement);
		m_writer.WriteStartElement("book-title").WriteCharacters(m_metadataReplacement->title).WriteEndElement();
	}

	void WriteSeries()
	{
		assert(m_metadataReplacement);
		if (m_metadataReplacement->series.isEmpty())
			return;

		const auto node = m_writer.Guard("sequence");
		node->WriteAttribute("name", m_metadataReplacement->series);
		if (m_metadataReplacement->seqNumber > 0)
			node->WriteAttribute("number", QString::number(m_metadataReplacement->seqNumber));
	}

	void WriteImage(const QString& imageFileName)
	{
		const auto data = [&] {
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
			const auto [recoded, mediaType] = Util::Recode(body);
			m_writer.WriteStartElement(BINARY).WriteAttribute(ID, name).WriteAttribute(CONTENT_TYPE, mediaType).WriteCharacters(QString::fromUtf8(recoded.toBase64())).WriteEndElement();
		}

		m_covers.clear();
	}

private:
	Util::XmlWriter                                     m_writer;
	Covers                                              m_covers;
	std::unique_ptr<const ILogicFactory::ExtractedBook> m_metadataReplacement;
	bool                                                m_hasError { false };
	bool                                                m_hasProgramUsed { false };
	ParseMode                                           m_currentMode { ParseMode::Common };

	std::unordered_map<ParseMode, void (SaxPrinter::*)()> m_replacers {
		{ ParseMode::Author, &SaxPrinter::WriteAuthor },
		{  ParseMode::Title,  &SaxPrinter::WriteTitle },
		{ ParseMode::Series, &SaxPrinter::WriteSeries },
	};
};

QByteArray RestoreImagesImpl(QIODevice& stream, Covers covers, std::unique_ptr<const ILogicFactory::ExtractedBook> metadataReplacement)
{
	QByteArray byteArray;
	QBuffer    buffer(&byteArray);
	buffer.open(QIODevice::WriteOnly);
	const SaxPrinter saxPrinter(stream, buffer, std::move(covers), std::move(metadataReplacement));
	return saxPrinter.HasError() ? QByteArray {} : byteArray;
}

void ConvertToGrayscale(QByteArray& srcImageBody, const int quality)
{
	const auto pixmap = Util::Decode(srcImageBody);
	if (pixmap.isNull())
		return;

	auto image = pixmap.toImage();
	if (image.pixelFormat().alphaUsage() == QPixelFormat::AlphaUsage::IgnoresAlpha)
	{
		image.convertTo(QImage::Format::Format_Grayscale8);
	}
	else
	{
		QImage alpha(image.width(), image.height(), QImage::Format::Format_Grayscale8);

		for (auto h = 0, H = image.height(); h < H; ++h)
		{
			auto* alphaBytes = alpha.scanLine(h);
			for (auto w = 0, W = image.width(); w < W; ++w, ++alphaBytes)
				*alphaBytes = static_cast<uchar>(qAlpha(image.pixel(w, h)));
		}

		image.convertTo(QImage::Format::Format_Grayscale8);
		image.setAlphaChannel(alpha);
	}

	QByteArray result;
	{
		QBuffer imageOutput(&result);
		if (!image.save(&imageOutput, Util::PNG, quality))
			return;
	}

	srcImageBody = std::move(result);
}

QByteArray
RestoreImagesImpl(QIODevice& stream, const QString& folder, const QString& fileName, const std::shared_ptr<const ISettings>& settings, std::unique_ptr<const ILogicFactory::ExtractedBook> metadataReplacement)
{
	Covers covers;
	ExtractBookImages(
		folder,
		fileName,
		[&covers](auto name, auto body) {
			covers.try_emplace(std::move(name), std::move(body));
			return false;
		},
		settings
	);
	if (covers.empty() && !metadataReplacement)
		return stream.readAll();

	if (auto byteArray = RestoreImagesImpl(stream, std::move(covers), std::move(metadataReplacement)); !byteArray.isEmpty())
		return byteArray;

	stream.seek(0);
	return stream.readAll();
}

bool ParseCover(const QString& folder, const QString& fileName, const ExtractBookImagesCallback& callback, bool& stop, const bool grayscale)
{
	if (!QFile::exists(folder))
		return false;

	const Zip  zip(folder);
	const auto fileList = zip.GetFileNameList();
	const auto file     = QFileInfo(fileName).completeBaseName();
	if (!fileList.contains(file))
		return false;

	const auto stream = zip.Read(file);
	auto       body   = stream->GetStream().readAll();
	if (grayscale)
		ConvertToGrayscale(body, 50);
	stop = callback(Global::COVER, std::move(body));
	return true;
}

void ParseImages(const QString& folder, const QString& fileName, const ExtractBookImagesCallback& callback, const bool grayscale)
{
	if (!QFile::exists(folder))
		return;

	const Zip  zip(folder);
	auto       fileList   = zip.GetFileNameList();
	const auto filePrefix = QString("%1/").arg(QFileInfo(fileName).completeBaseName());
	if (const auto [begin, end] = std::ranges::remove_if(
			fileList,
			[&](const auto& item) {
				return !item.startsWith(filePrefix) /*|| item == filePrefix*/;
			}
		);
	    begin != end)
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

constexpr const char* EXTENSIONS[] { "zip", "7z" };

void ExtractBookImagesCoverImpl(const QFileInfo& fileInfo, const QString& fileName, const ExtractBookImagesCallback& callback, const std::shared_ptr<const ISettings>& settings)
{
	if (settings && settings->Get(Constant::Settings::EXPORT_REMOVE_COVER_KEY, false))
		return;

	bool stop = false;
	for (const auto* ext : EXTENSIONS)
	{
		TRY("parse cover", [&] {
			ParseCover(
				QString("%1/%2/%3.%4").arg(fileInfo.dir().path(), Global::COVERS, fileInfo.completeBaseName(), ext),
				fileName,
				callback,
				stop,
				settings && settings->Get(Constant::Settings::EXPORT_GRAYSCALE_COVER_KEY, false)
			);
			return true;
		});
	}
}

void ExtractBookImagesImagesImpl(const QFileInfo& fileInfo, const QString& fileName, const ExtractBookImagesCallback& callback, const std::shared_ptr<const ISettings>& settings)
{
	if (settings && settings->Get(Constant::Settings::EXPORT_REMOVE_IMAGES_KEY, false))
		return;

	for (const auto* ext : EXTENSIONS)
	{
		TRY("parse cover", [&] {
			ParseImages(
				QString("%1/%2/%3.%4").arg(fileInfo.dir().path(), Global::IMAGES, fileInfo.completeBaseName(), ext),
				fileName,
				callback,
				settings && settings->Get(Constant::Settings::EXPORT_GRAYSCALE_IMAGES_KEY, false)
			);
			return true;
		});
	}
}

} // namespace

namespace HomeCompa::Flibrary
{

QByteArray
RestoreImages(QIODevice& input, const QString& folder, const QString& fileName, const std::shared_ptr<const ISettings>& settings, std::unique_ptr<const ILogicFactory::ExtractedBook> metadataReplacement)
{
	return RestoreImagesImpl(input, folder, fileName, settings, std::move(metadataReplacement));
}

void ExtractBookImages(const QString& folder, const QString& fileName, const ExtractBookImagesCallback& callback, const std::shared_ptr<const ISettings>& settings)
{
	const QFileInfo fileInfo(folder);
	ExtractBookImagesCoverImpl(fileInfo, fileName, callback, settings);
	ExtractBookImagesImagesImpl(fileInfo, fileName, callback, settings);
}

} // namespace HomeCompa::Flibrary
