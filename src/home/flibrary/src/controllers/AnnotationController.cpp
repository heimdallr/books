#include <fstream>
#include <mutex>

#include <QBuffer>
#include <QCursor>
#include <QGuiApplication>
#include <QTimer>
#include <QXmlStreamReader>

#include <plog/Log.h>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "util/IExecutor.h"
#include "util/executor/factory.h"

#include "ModelControllers/IBooksModelControllerObserver.h"
#include "ModelControllers/NavigationSource.h"

#include "models/Book.h"

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
		XML_PARSE_MODE_ITEM(keywords)   \

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

constexpr auto TABLE_STRING_TEMPLATE = "<tr><td>%1: </td><td>%2</td></tr>";
constexpr auto LINK_TEMPLATE = R"(<a href="%1|%2">%3</a>)";

QString GetBookInfoTable(const Book & book)
{
	return QString("<table>")
		+ QString(TABLE_STRING_TEMPLATE).arg(QCoreApplication::translate("Header", "File")).arg(book.FileName)
		+ QString(TABLE_STRING_TEMPLATE).arg(QCoreApplication::translate("Header", "Size")).arg(QString("%L1").arg(book.Size))
		+ QString(TABLE_STRING_TEMPLATE).arg(QCoreApplication::translate("Header", "Updated")).arg(book.UpdateDate)
		+ QString("</table>");
}

QString GetBookLinksTable(const Book & book)
{
	const auto addLinks = [&] (const auto & container, const char * name)
	{
		if (container.empty())
			return QString();

		QStringList list;
		std::ranges::transform(container, std::back_inserter(list), [name] (const auto & item)
		{
			return QString(LINK_TEMPLATE).arg(name).arg(item.first).arg(item.second);
		});
		return QString(TABLE_STRING_TEMPLATE).arg(QCoreApplication::translate(Constant::NavigationSourceTranslationContext, name)).arg(list.join(", "));
	};

	return QString("<table>")
#define VIEW_SOURCE_NAVIGATION_MODEL_ITEM(NAME) + addLinks(book.NAME, #NAME)
		VIEW_SOURCE_NAVIGATION_MODEL_ITEMS_XMACRO
#undef	VIEW_SOURCE_NAVIGATION_MODEL_ITEM
		+ QString("</table>");
}

struct AnnotationData
{
	QString annotation;
	QStringList keywords;
	std::vector<QString> covers;
	int coverIndex { -1 };
};

struct XmlParser final
	: QXmlStreamReader
{
	AnnotationData data;

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
			data.annotation.append(QString("<%1>").arg(nodeName));
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
				data.annotation.append(QString("</%1>").arg(nodeName));
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
			{ XmlParseMode::keywords, &XmlParser::OnCharactersKeywords },
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
			data.coverIndex = static_cast<int>(data.covers.size());

		data.covers.emplace_back(QString("data:%1;base64,").arg(contentType));
	}

	void OnCharactersAnnotation()
	{
		data.annotation.append(text());
	}

	void OnCharactersBinary()
	{
		data.covers.back().append(text());
	}

	void OnCharactersKeywords()
	{
		data.keywords << text().toString().split(",", Qt::SkipEmptyParts);
	}

private:
	XmlParseMode m_mode { XmlParseMode::unknown };
	QString m_coverPageId;
};

void GetGroupInfo(DB::IDatabase & db, Book & book)
{
	constexpr auto queryText =
		"select g.GroupID, g.Title "
		"from Groups_User g "
		"join Groups_List_User gl on gl.GroupID = g.GroupID and gl.BookID = ? "
		;
	const auto query = db.CreateQuery(queryText);
	query->Bind(0, book.Id);
	for (query->Execute(); !query->Eof(); query->Next())
		book.Groups.emplace(query->Get<long long>(0), query->Get<const char *>(1));
}

AnnotationData ParseAnnotation(const std::filesystem::path & folder, const Book & book)
{
	if (!exists(folder))
	{
		PLOGW << folder.generic_string() << " not found";
		return {};
	}

	QuaZip zip(QString::fromStdWString(folder));
	zip.open(QuaZip::Mode::mdUnzip);
	zip.setCurrentFile(book.FileName);

	QuaZipFile zipFile(&zip);
	zipFile.open(QIODevice::ReadOnly);

	XmlParser parser(zipFile);
	parser.Parse();

	return std::move(parser.data);
}

}

class AnnotationController::Impl
	: virtual public IBooksModelControllerObserver
{
public:
	explicit Impl(const AnnotationController & self, DB::IDatabase & db, std::filesystem::path rootFolder)
		: m_self(self)
		, m_db(db)
		, m_rootFolder(std::move(rootFolder))
	{
		m_annotationTimer.setSingleShot(true);
		m_annotationTimer.setInterval(std::chrono::milliseconds(250));
		connect(&m_annotationTimer, &QTimer::timeout, [&] { ExtractAnnotation(); });
	}

	void CoverNext()
	{
		if (m_covers.size() < 2)
			return;

		if (++m_coverIndex >= GetCoverCount())
			m_coverIndex = 0;

		emit m_self.CoverChanged();
	}

	void CoverPrev()
	{
		if (m_covers.size() < 2)
			return;

		if (--m_coverIndex < 0)
			m_coverIndex = GetCoverCount() - 1;

		emit m_self.CoverChanged();
	}

	int GetCoverCount() const noexcept
	{
		return static_cast<int>(m_covers.size());
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

	const Book & GetCurrentBook() const noexcept
	{
		return m_book;
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
		(*m_executor)({ "Get annotation", [&, book = std::move(m_book)]() mutable
		{
			const auto folder = (m_rootFolder / book.Folder.toStdWString()).make_preferred();
			const auto file = book.FileName.toStdString();

			auto stub = [&]
			{
				emit m_self.AnnotationChanged();
				emit m_self.HasCoverChanged();
				emit m_self.CoverChanged();
				emit m_self.CoverCountChanged();
			};

			{
				std::lock_guard lock(m_guard);
				m_annotation.clear();
				m_covers.clear();
				m_coverIndex = -1;
			}

			if (book.IsDictionary || book.Id < 0)
				return stub;

			try
			{
				GetGroupInfo(m_db, book);
				auto annotationData = ParseAnnotation(folder, book);
				UpdateAnnotation(std::move(annotationData), book);
			}
			catch (const std::exception & ex)
			{
				PLOGE << ex.what();
			}

			return stub;
		} }, 3);
	}

	void UpdateAnnotation(AnnotationData annotationData, const Book & book)
	{
		std::lock_guard lock(m_guard);
		m_annotation = std::move(annotationData.annotation);
		if (!annotationData.keywords.isEmpty())
			m_annotation += QString("<p>%1: %2</p>").arg(QCoreApplication::translate("Annotation", "Keywords")).arg(annotationData.keywords.join(", "));
		m_annotation += QString("<p>%1</p>").arg(GetBookLinksTable(book));
		m_annotation += QString("<p>%1</p>").arg(GetBookInfoTable(book));

		m_covers = std::move(annotationData.covers);
		m_coverIndex
			= m_covers.empty() ? -1
			: annotationData.coverIndex >= 0 ? annotationData.coverIndex
			: 0
			;
	}

private:
	const AnnotationController & m_self;
	DB::IDatabase & m_db;
	const std::filesystem::path m_rootFolder;
	PropagateConstPtr<Util::IExecutor> m_executor { Util::ExecutorFactory::Create(Util::ExecutorImpl::Async, {[]{}, []{ QGuiApplication::setOverrideCursor(Qt::BusyCursor); }, [] { QGuiApplication::restoreOverrideCursor(); }}) };
	QTimer m_annotationTimer;

	Book m_book;

	mutable std::mutex m_guard;
	QString m_annotation;
	std::vector<QString> m_covers;
	int m_coverIndex { -1 };
};

AnnotationController::AnnotationController(DB::IDatabase & db, std::filesystem::path rootFolder)
	: m_impl(*this, db, std::move(rootFolder))
{
	PLOGD << "AnnotationController created";
}

AnnotationController::~AnnotationController()
{
	PLOGD << "AnnotationController destroyed";
}

void AnnotationController::CoverNext()
{
	m_impl->CoverNext();
}

void AnnotationController::CoverPrev()
{
	m_impl->CoverPrev();
}

long long AnnotationController::GetCurrentBookId() const noexcept
{
	return m_impl->GetCurrentBook().Id;
}

IBooksModelControllerObserver * AnnotationController::GetBooksModelControllerObserver()
{
	return m_impl.get();
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

int AnnotationController::GetCoverCount() const noexcept
{
	return m_impl->GetCoverCount();
}

}
