#include <mutex>

#include <QCursor>
#include <QGuiApplication>
#include <QTextCodec>
#include <QTimer>

#include "fnd/FindPair.h"

#include "util/executor.h"
#include "util/executor/factory.h"

#include "ZipLib/ZipFile.h"

#include "SimpleSaxParser/SaxParser.h"

#include "ModelControllers/BooksModelControllerObserver.h"

#include "AnnotationController.h"

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

bool IsNewLine(const std::string & name)
{
	return false
		|| name == "p"
		|| name == "empty-line"
		;
}

struct SaxHandler : XSPHandler
{
	std::string annotation;
	std::vector<QString> covers;
	int coverIndex { -1 };

private: // XSPHandler
	void OnEncoding([[maybe_unused]] const std::string & name) override
	{
		m_textCodec = QTextCodec::codecForName(name.data());
	}

	void OnElementBegin(const std::string & name) override
	{
		if (m_mode == XmlParseMode::annotation)
		{
			if (IsNewLine(name))
				annotation.append("    ");
			return;
		}

		const auto mode = FindSecond(g_parseModes, name.data(), XmlParseMode::unknown, PszComparer {});
		if (mode == XmlParseMode::image && m_mode != XmlParseMode::coverpage)
			return;

		m_mode = mode;
	}

	void OnElementEnd(const std::string & name) override
	{
		if (m_mode == XmlParseMode::annotation)
		{
			if (IsNewLine(name))
				annotation.append("\n\n");

			if (name != "annotation")
				return;
		}

		m_mode = XmlParseMode::unknown;
	}

	void OnText(const std::string & value) override
	{
		if (m_mode == XmlParseMode::annotation)
			annotation.append(m_textCodec->toUnicode(value.data(), static_cast<int>(value.size())).toStdString());

		if (m_mode == XmlParseMode::binary)
			ProcessBinary(value);
	}

	void OnAttribute(const std::string & name, const std::string & value) override
	{
		switch (m_mode)
		{
			case XmlParseMode::binary:
				if (name == CONTENT_TYPE)
				{
					m_binaryContentType = value;
					break;
				}
				if (name == ID)
				{
					m_binaryId = value;
					break;
				}
				break;

			case XmlParseMode::image:
				if (name == L_HREF)
				{
					m_coverPageId = value;
					if (const auto pos = m_coverPageId.find_first_not_of('#'); pos != std::string::npos)
						m_coverPageId.erase(m_coverPageId.cbegin(), std::next(m_coverPageId.cbegin(), pos));
				}
				break;

			default:
				break;
		}
	}

private:
	void ProcessBinary(const std::string & value)
	{
		if (m_binaryId == m_coverPageId)
			coverIndex = static_cast<int>(covers.size());

		covers.emplace_back(QString("data:%1;base64,").arg(m_binaryContentType.data()).append(value.data()));
	}

private:
	XmlParseMode m_mode { XmlParseMode::unknown };
	std::string m_binaryContentType;
	std::string m_coverPageId;
	std::string m_binaryId;
	QTextCodec * m_textCodec { QTextCodec::codecForLocale() };
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

	const QString & GetAnnotation() const
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
	void HandleBookChanged(const std::string & folder, const std::string & file) override
	{
		m_folderTmp = folder;
		m_fileTmp = file;
		m_annotationTimer.start();
	}

	void HandleModelReset() override
	{
	}

private:
	void ExtractAnnotation()
	{
		if (m_folder == m_folderTmp && m_file == m_fileTmp)
			return;

		m_folder = m_folderTmp;
		m_file = m_fileTmp;

		(*m_executor)({ "Get annotation", [&, folder = (m_rootFolder / m_folder).make_preferred(), file = m_file]
		{
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

			SaxParser parser;
			SaxHandler handler;
			try
			{
				parser.Parse(stream, &handler);

				std::lock_guard lock(m_guard);
				m_annotation = QString::fromStdString(handler.annotation);
				m_covers = std::move(handler.covers);
				m_coverIndex
					= m_covers.empty() ? -1
					: handler.coverIndex >= 0 ? handler.coverIndex
					: 0
					;
			}
			catch (SaxParserException &)
			{
			}

			return stub;
		} }, 3);
	}

private:
	const AnnotationController & m_self;
	PropagateConstPtr<Util::Executor> m_executor { Util::ExecutorFactory::Create(Util::ExecutorImpl::Async, {[]{}, []{ QGuiApplication::setOverrideCursor(Qt::BusyCursor); }, [] { QGuiApplication::restoreOverrideCursor(); }}) };
	QTimer m_annotationTimer;
	std::filesystem::path m_rootFolder;

	std::string m_folder, m_folderTmp;
	std::string m_file, m_fileTmp;

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

const QString & AnnotationController::GetAnnotation() const
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
