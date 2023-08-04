#include "ArchiveParser.h"

#include <quazip>
#include <QXmlStreamReader>
#include <plog/Log.h>

#include "fnd/FindPair.h"
#include "interface/logic/ICollectionController.h"

#include "data/DataItem.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTENT_TYPE = "content-type";
constexpr auto ID = "id";
constexpr auto L_HREF = "l:href";

#define XML_PARSE_MODE_ITEMS_X_MACRO    \
		XML_PARSE_MODE_ITEM(annotation) \
		XML_PARSE_MODE_ITEM(binary)     \
		XML_PARSE_MODE_ITEM(coverpage)  \
		XML_PARSE_MODE_ITEM(image)      \
		XML_PARSE_MODE_ITEM(keywords)   \

enum class XmlParseMode
{
	unknown = -1,
#define XML_PARSE_MODE_ITEM(NAME) NAME,
	XML_PARSE_MODE_ITEMS_X_MACRO
#undef	XML_PARSE_MODE_ITEM
};

constexpr std::pair<const char *, XmlParseMode> g_parseModes[]
{
#define XML_PARSE_MODE_ITEM(NAME) { #NAME, XmlParseMode::NAME },
		XML_PARSE_MODE_ITEMS_X_MACRO
#undef	XML_PARSE_MODE_ITEM
};

class XmlParser final
	: QXmlStreamReader
{
public:
	explicit XmlParser(QIODevice & ioDevice)
		: QXmlStreamReader(&ioDevice)
	{
//		[[maybe_unused]]const auto data = ioDevice.readAll();
	}

	ArchiveParser::Data Parse()
	{
		using ParseType = void(XmlParser::*)();
		static constexpr std::pair<TokenType, ParseType> s_parsers[]
		{
			{ StartElement, &XmlParser::OnStartElement },
			{ EndElement, &XmlParser::OnEndElement },
			{ Characters, &XmlParser::OnCharacters },
		};

		while (!atEnd() && !hasError())
		{
			const auto token = readNext();
			if (token == QXmlStreamReader::Invalid)
				PLOGE << error() << ":" << errorString();

			const auto parser = FindSecond(s_parsers, token, &XmlParser::Stub, std::equal_to<>{});
			std::invoke(parser, this);
		}

		return m_data;
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
			m_data.annotation.append(QString("<%1>").arg(nodeName));
			return;
		}

		const auto mode = FindSecond(g_parseModes, nodeName.toUtf8(), XmlParseMode::unknown, PszComparer {});
		if (mode == XmlParseMode::image && m_mode != XmlParseMode::coverpage)
			return;

		m_mode = mode;

		using ParseType = void(XmlParser::*)();
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
			if (nodeName.compare("annotation") != 0)
			{
				m_data.annotation.append(QString("</%1>").arg(nodeName));
				return;
			}
		}

		m_mode = XmlParseMode::unknown;
	}

	void OnCharacters()
	{
		using ParseType = void(XmlParser::*)();
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
			m_data.coverIndex = static_cast<int>(m_data.covers.size());
	}

	void OnCharactersAnnotation()
	{
		m_data.annotation.append(text());
	}

	void OnCharactersBinary()
	{
		m_data.covers.push_back(QByteArray::fromBase64(text().toUtf8()));
	}

	void OnCharactersKeywords()
	{
		std::ranges::copy(text().toString().split(",", SkipEmptyParts), std::back_inserter(m_data.keywords));
	}

private:
	XmlParseMode m_mode { XmlParseMode::unknown };
	QString m_coverPageId;
	ArchiveParser::Data m_data;
};

}

class ArchiveParser::Impl
{
public:
	explicit Impl(std::shared_ptr<ICollectionController> collectionController)
		: m_collectionController(std::move(collectionController))
	{
	}

	[[nodiscard]] Data Parse(const IDataItem & book) const
	{
		const auto folder = QString("%1/%2").arg(m_collectionController->GetActiveCollection().folder, book.GetRawData(BookItem::Column::Folder));
		QuaZip zip(folder);
		zip.open(QuaZip::Mode::mdUnzip);
		zip.setCurrentFile(book.GetRawData(BookItem::Column::FileName));

		QuaZipFile zipFile(&zip);
		zipFile.open(QIODevice::ReadOnly);

		XmlParser parser(zipFile);
		return parser.Parse();
	}

private:
	PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
};

ArchiveParser::ArchiveParser(std::shared_ptr<ICollectionController> collectionController)
	: m_impl(std::move(collectionController))
{
	PLOGD << "AnnotationParser created";
}

ArchiveParser::~ArchiveParser()
{
	PLOGD << "AnnotationParser destroyed";
}

ArchiveParser::Data  ArchiveParser::Parse(const IDataItem & dataItem) const
{
	return m_impl->Parse(dataItem);
}
