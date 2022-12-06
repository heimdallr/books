#include <mutex>

#include <QCursor>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QTextCodec>
#include <QTimer>
#include <QXmlStreamReader>


#include "fnd/FindPair.h"

#include "util/executor.h"
#include "util/executor/factory.h"

#include "ZipLib/ZipFile.h"

#include "ModelControllers/BooksModelControllerObserver.h"

#include "models/Book.h"

#include "AnnotationController.h"

#include <plog/Log.h>

namespace HomeCompa::Flibrary {

namespace {

constexpr auto CONTENT_TYPE = "content-type";
constexpr auto ID = "id";
constexpr auto L_HREF = "l:href";

#define XML_PARSE_MODE_ITEMS_XMACRO     \
		XML_PARSE_MODE_ITEM(annotation) \
		XML_PARSE_MODE_ITEM(binary)     \
		XML_PARSE_MODE_ITEM(coverpage)  \
		XML_PARSE_MODE_ITEM(image)      \

enum class XmlParseMode
{
		unknown = -1,
#define XML_PARSE_MODE_ITEM(NAME) NAME,
		XML_PARSE_MODE_ITEMS_XMACRO
#undef	XML_PARSE_MODE_ITEM
};

constexpr std::pair<const char *, XmlParseMode> g_parseModes[]
{
#define XML_PARSE_MODE_ITEM(NAME) { #NAME, XmlParseMode::NAME },
		XML_PARSE_MODE_ITEMS_XMACRO
#undef	XML_PARSE_MODE_ITEM
};

QString GetBookInfoTable(const Book & book)
{
	return QString("<table>")
		+ QCoreApplication::translate("Annotation", "<tr><td>Archive: </td><td>%1</td></tr>").arg(book.Folder)
		+ QCoreApplication::translate("Annotation", "<tr><td>File: </td><td>%1</td></tr>").arg(book.FileName)
		+ QCoreApplication::translate("Annotation", "<tr><td>Updated: </td><td>%1</td></tr>").arg(book.UpdateDate)
		+ QString("</table>");
}

class IODeviceStdStreamWrapper final
	: public QIODevice
{
public:
	explicit IODeviceStdStreamWrapper(std::istream & stream)
		: m_stream(stream)
	{
	}

private: // QIODevice
	qint64 readData(char * const data, const qint64 maxLength) override
	{
		m_stream.read(data, maxLength);
		return m_stream.gcount();
	}
	qint64 readLineData(char * const data, const qint64 maxLength) override
	{
		m_stream.getline(data, maxLength);
		return m_stream.gcount();
	}
	qint64 writeData(const char *, const qint64) override
	{
		throw std::runtime_error("Cannot write to read only stream");
	}

	bool atEnd() const override
	{
		return m_stream.eof();
	}
	qint64 bytesAvailable() const override
	{
		return size() - pos();
	}
	qint64 bytesToWrite() const override
	{
		return 0;
	}
	bool canReadLine() const override
	{
		return false;
	}
	void close() override
	{
		QIODevice::close();
	}
	bool isSequential() const override
	{
		return false;
	}
	bool open(OpenMode mode) override
	{
		if (mode & (WriteOnly | Append | Truncate | NewOnly))
			throw std::runtime_error("Read only expected");

		return m_stream.good() && QIODevice::open(mode);
	}
	qint64 pos() const override
	{
		return m_stream.tellg();
	}
	bool reset() override
	{
		return seek(0);
	}
	bool seek(qint64 pos) override
	{
		m_stream.seekg(pos);
		if (m_stream.eof())
			return false;

		return QIODevice::seek(pos);
	}
	qint64 size() const override
	{
		if (m_size < 0)
		{
			const auto currentPosition = pos();
			m_stream.seekg(0, std::ios_base::end);
			m_size = pos();
			m_stream.seekg(currentPosition, std::ios_base::beg);
		}

		return m_size;
	}

	bool waitForBytesWritten(int) override
	{
		return true;
	}
	bool waitForReadyRead(int) override
	{
		return true;
	}

private:
	std::istream & m_stream;
	mutable qint64 m_size { -1 };
};

struct XmlParser final
	: QXmlStreamReader
{
	QString annotation;
	std::vector<QString> covers;
	int coverIndex { -1 };

	explicit XmlParser(QIODevice & ioDevice)
		: QXmlStreamReader(&ioDevice)
	{
	}

	void Parse()
	{
		using ParseType = void(XmlParser:: *)();
		static constexpr std::pair<TokenType, ParseType> s_parsers[]
		{
			{ StartElement, &XmlParser::OnStartElement },
			{ EndElement, &XmlParser::OnEndElement },
			{ Characters, &XmlParser::OnCharacters },
		};

		while (!atEnd() && !hasError())
		{
			const auto token = readNext();
			const auto parser = FindSecond(s_parsers, token, &XmlParser::Stub, std::equal_to<>{});
			std::invoke(parser, this);
		}
	}

private:
	void Stub()
	{
	}

	void OnStartElement()
	{
		const auto nodeName = name();
		if (m_mode == XmlParseMode::annotation)
		{
			annotation.append(QString("<%1>").arg(nodeName));
			return;
		}

		const auto mode = FindSecond(g_parseModes, nodeName.toUtf8(), XmlParseMode::unknown, PszComparer {});
		if (mode == XmlParseMode::image && m_mode != XmlParseMode::coverpage)
			return;

		m_mode = mode;

		using ParseType = void(XmlParser:: *)();
		static constexpr std::pair<XmlParseMode, ParseType> s_parsers[]
		{
			{ XmlParseMode::image, &XmlParser::OnStartElementCoverPageImage },
			{ XmlParseMode::binary, &XmlParser::OnStartElementBinary },
		};

		const auto parser = FindSecond(s_parsers, m_mode, &XmlParser::Stub, std::equal_to<>{});
		std::invoke(parser, this);
	}

	void OnEndElement()
	{
		const auto nodeName = name();
		if (m_mode == XmlParseMode::annotation)
		{
			if (nodeName != "annotation")
			{
				annotation.append(QString("</%1>").arg(nodeName));
				return;
			}
		}

		m_mode = XmlParseMode::unknown;
	}

	void OnCharacters()
	{
		using ParseType = void(XmlParser:: *)();
		static constexpr std::pair<XmlParseMode, ParseType> s_parsers[]
		{
			{ XmlParseMode::annotation, &XmlParser::OnCharactersAnnotation },
			{ XmlParseMode::binary, &XmlParser::OnCharactersBinary },
		};

		const auto parser = FindSecond(s_parsers, m_mode, &XmlParser::Stub, std::equal_to<>{});
		std::invoke(parser, this);
	}

	void OnStartElementCoverPageImage()
	{
		auto coverPageId = attributes().value(L_HREF);
		while (!coverPageId.isEmpty() && coverPageId.startsWith('#'))
			coverPageId = coverPageId.right(coverPageId.length() - 1);
		m_coverPageId = coverPageId.toString();
	}

	void OnStartElementBinary()
	{
		const auto attrs = attributes();
		const auto binaryId = attrs.value(ID), contentType = attrs.value(CONTENT_TYPE);
		if (m_coverPageId == binaryId)
			coverIndex = static_cast<int>(covers.size());

		covers.emplace_back(QString("data:%1;base64,").arg(contentType));
	}

	void OnCharactersAnnotation()
	{
		annotation.append(text());
	}

	void OnCharactersBinary()
	{
		covers.back().append(text());
	}

private:
	XmlParseMode m_mode { XmlParseMode::unknown };
	QString m_coverPageId;
};

}

class AnnotationController::Impl
	: virtual public BooksModelControllerObserver
{
public:
	explicit Impl(const AnnotationController & self)
		: m_self(self)
	{
		m_annotationTimer.setSingleShot(true);
		m_annotationTimer.setInterval(std::chrono::milliseconds(250));
		connect(&m_annotationTimer, &QTimer::timeout, [&] { ExtractAnnotation(); });
	}

	void CoverNext()
	{
		if (m_covers.size() < 2)
			return;

		if (++m_coverIndex >= static_cast<int>(m_covers.size()))
			m_coverIndex = 0;

		emit m_self.CoverChanged();
	}

	void CoverPrev()
	{
		if (m_covers.size() < 2)
			return;

		if (--m_coverIndex < 0)
			m_coverIndex = static_cast<int>(m_covers.size()) - 1;

		emit m_self.CoverChanged();
	}

	void SetRootFolder(std::filesystem::path rootFolder)
	{
		m_rootFolder = std::move(rootFolder);
	}

	QString GetAnnotation() const
	{
		std::lock_guard lock(m_guard);
		return m_annotation;
	}

	bool GetHasCover() const
	{
		std::lock_guard lock(m_guard);
		return !m_covers.empty();
	}

	const QString & GetCover() const
	{
		assert(m_coverIndex >= 0 && m_coverIndex < static_cast<int>(std::size(m_covers)));
		return m_covers[m_coverIndex];
	}


private: // BooksModelControllerObserver
	void HandleBookChanged(const Book & book) override
	{
		m_book = book;
		m_annotationTimer.start();
	}

	void HandleModelReset() override
	{
	}

private:
	void ExtractAnnotation()
	{
		if (m_book.IsDictionary || m_book.Id < 0)
			return;

		(*m_executor)({ "Get annotation", [&, book = std::move(m_book)]
		{
			const auto folder = (m_rootFolder / book.Folder.toStdWString()).make_preferred();
			const auto file = book.FileName.toStdString();

			auto stub = [&]
			{
				emit m_self.AnnotationChanged();
				emit m_self.HasCoverChanged();
				emit m_self.CoverChanged();
			};

			{
				std::lock_guard lock(m_guard);
				m_annotation.clear();
				m_covers.clear();
				m_coverIndex = -1;
			}

			if (!exists(folder))
				return stub;

			const auto archive = ZipFile::Open(folder.generic_string());
			if (!archive)
				return stub;

			const auto entry = archive->GetEntry(file);
			if (!entry)
				return stub;

			auto * const stream = entry->GetDecompressionStream();
			if (!stream)
				return stub;

			std::unique_ptr<QIODevice> ioDevice = std::make_unique<IODeviceStdStreamWrapper>(*stream);
			ioDevice->open(QIODevice::ReadOnly);
			XmlParser parser(*ioDevice);

			try
			{
				parser.Parse();

				std::lock_guard lock(m_guard);
				m_annotation = std::move(parser.annotation);
				m_annotation += QString("%1%2%3").arg(m_annotation.isEmpty() ? "" : "<p>").arg(GetBookInfoTable(book)).arg(m_annotation.isEmpty() ? "" : "</p>");
				m_covers = std::move(parser.covers);
				m_coverIndex
					= m_covers.empty() ? -1
					: parser.coverIndex >= 0 ? parser.coverIndex
					: 0
					;
			}
			catch (const std::exception & ex)
			{
				PLOGE << ex.what();
			}

			return stub;
		} }, 3);
	}

private:
	const AnnotationController & m_self;
	PropagateConstPtr<Util::Executor> m_executor { Util::ExecutorFactory::Create(Util::ExecutorImpl::Async, {[]{}, []{ QGuiApplication::setOverrideCursor(Qt::BusyCursor); }, [] { QGuiApplication::restoreOverrideCursor(); }}) };
	QTimer m_annotationTimer;
	std::filesystem::path m_rootFolder;

	Book m_book;

	mutable std::mutex m_guard;
	QString m_annotation;
	std::vector<QString> m_covers;
	int m_coverIndex { -1 };
};

AnnotationController::AnnotationController()
	: m_impl(*this)
{
}

AnnotationController::~AnnotationController() = default;

void AnnotationController::CoverNext()
{
	m_impl->CoverNext();
}

void AnnotationController::CoverPrev()
{
	m_impl->CoverPrev();
}

BooksModelControllerObserver * AnnotationController::GetBooksModelControllerObserver()
{
	return m_impl.get();
}

void AnnotationController::SetRootFolder(std::filesystem::path rootFolder)
{
	m_impl->SetRootFolder(std::move(rootFolder));
}

QString AnnotationController::GetAnnotation() const
{
	return m_impl->GetAnnotation();
}

bool AnnotationController::GetHasCover() const
{
	return m_impl->GetHasCover();
}

const QString & AnnotationController::GetCover() const
{
	return m_impl->GetCover();
}

}
