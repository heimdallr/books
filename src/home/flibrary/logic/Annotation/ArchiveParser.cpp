#include "ArchiveParser.h"

#include <QFile>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include <plog/Log.h>

#include "fnd/FindPair.h"
#include "interface/constants/Localization.h"
#include "interface/logic/ICollectionController.h"

#include "data/DataItem.h"
#include "shared/ImageRestore.h"
#include "shared/ZipProgressCallback.h"

#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto CONTEXT = "Annotation";
constexpr auto CONTENT = QT_TRANSLATE_NOOP("Annotation", "Content");
TR_DEF

constexpr auto ID = "id";
constexpr auto L_HREF = "l:href";
constexpr auto A = "a";
constexpr auto ANNOTATION = "FictionBook/description/title-info/annotation";
constexpr auto ANNOTATION_P = "FictionBook/description/title-info/annotation/p";
constexpr auto ANNOTATION_HREF = "FictionBook/description/title-info/annotation/a";
constexpr auto ANNOTATION_HREF_P = "FictionBook/description/title-info/annotation/p/a";
constexpr auto KEYWORDS = "FictionBook/description/title-info/keywords";
constexpr auto BINARY = "FictionBook/binary";
constexpr auto COVERPAGE_IMAGE = "FictionBook/description/title-info/coverpage/image";
constexpr auto SECTION = "section";
constexpr auto SECTION_TITLE = "section/title";
constexpr auto SECTION_TITLE_P = "section/title/p";
constexpr auto SECTION_TITLE_P_STRONG = "section/title/p/strong";
constexpr auto EPIGRAPH = "FictionBook/body/epigraph";
constexpr auto EPIGRAPH_P = "FictionBook/body/epigraph/p";
constexpr auto EPIGRAPH_AUTHOR = "FictionBook/body/epigraph/text-author";

struct PszComparerEndsWithCaseInsensitive
{
	bool operator()(const std::string_view lhs, const std::string_view rhs) const
	{
		if (lhs.size() < rhs.size())
			return false;

		const auto * lp = lhs.data() + (lhs.size() - rhs.size()), * rp = rhs.data();
		while (*lp && *rp && std::tolower(*lp++) == std::tolower(*rp++))
			;

		return !*lp && !*rp;
	}
};


class XmlStack
{
public:
	void Push(const QStringView tag)
	{
		m_data.push_back(tag.toString());
		m_key.reset();
	}

	void Pop(const QStringView tag)
	{
		assert(!m_data.isEmpty());
		if (tag != m_data.back())
			return;

		m_data.pop_back();
		m_key.reset();
	}

	const QString & ToString() const
	{
		if (!m_key)
			m_key = m_data.join('/');

		return *m_key;
	}

private:
	mutable std::optional<QString> m_key;
	QStringList m_data;
};

class XmlParser final
	: QXmlStreamReader
{
	using ParseElementFunction = void(XmlParser::*)();
	using ParseElementItem = std::pair<const char *, ParseElementFunction>;

public:
	explicit XmlParser(QIODevice & ioDevice)
		: QXmlStreamReader(&ioDevice)
	{
		m_data.content->SetData(Tr(CONTENT), NavigationItem::Column::Title);
	}

	ArchiveParser::Data Parse(const QString & rootFolder, const IDataItem & book)
	{
		static constexpr std::pair<TokenType, ParseElementFunction> PARSERS[]
		{
			{ StartElement, &XmlParser::OnStartElement },
			{ EndElement, &XmlParser::OnEndElement },
			{ Characters, &XmlParser::OnCharacters },
		};

		while (!atEnd() && !hasError())
		{
			const auto token = readNext();
			if (token == Invalid)
				PLOGE << error() << ":" << errorString();

			const auto parser = FindSecond(PARSERS, token, &XmlParser::Stub<>);
			std::invoke(parser, this);
		}

		update_covers(rootFolder, book);

		for (auto&& [name, bytes] : m_covers)
		{
			if (bytes.isNull())
				continue;

			if (name == m_coverpage)
				m_data.coverIndex = static_cast<int>(m_data.covers.size());
			m_data.covers.push_back(std::move(bytes));
		}

		return m_data;
	}

private:
	template<typename... ARGS>
	// ReSharper disable once CppMemberFunctionMayBeStatic
	void Stub(ARGS &&...)
	{
	}

	void OnStartElement()
	{
		const auto tag = name();
		m_stack.Push(tag);

		if (tag.compare(A, Qt::CaseInsensitive) == 0)
			m_href = attributes().value(L_HREF).toString();

		static constexpr ParseElementItem PARSERS[]
		{
			{ COVERPAGE_IMAGE, &XmlParser::OnStartElementCoverpageImage },
			{ BINARY, &XmlParser::OnStartElementBinary },
			{ SECTION, &XmlParser::OnStartElementSection },
		};

		Parse(PARSERS);
	}

	void OnEndElement()
	{
		static constexpr ParseElementItem PARSERS[]
		{
			{ SECTION, &XmlParser::OnEndElementSection },
		};

		Parse(PARSERS);

		m_stack.Pop(name());
	}

	void OnCharacters()
	{
		auto textValue = text().toString().simplified();
		if (textValue.isEmpty())
			return;

		using ParseCharacterFunction = void(XmlParser::*)(QString&&);
		using ParseCharacterItem = std::pair<const char *, ParseCharacterFunction>;
		static constexpr ParseCharacterItem PARSERS[]
		{
			{ ANNOTATION, &XmlParser::ParseAnnotation },
			{ ANNOTATION_P, &XmlParser::ParseAnnotation },
			{ ANNOTATION_HREF, &XmlParser::ParseAnnotationHref },
			{ ANNOTATION_HREF_P, &XmlParser::ParseAnnotationHref },
			{ KEYWORDS, &XmlParser::ParseKeywords },
			{ BINARY, &XmlParser::ParseBinary },
			{ SECTION_TITLE, &XmlParser::ParseSectionTitle },
			{ SECTION_TITLE_P, &XmlParser::ParseSectionTitle },
			{ SECTION_TITLE_P_STRONG, &XmlParser::ParseSectionTitle },
			{ EPIGRAPH, &XmlParser::ParseEpigraph },
			{ EPIGRAPH_P, &XmlParser::ParseEpigraph },
			{ EPIGRAPH_AUTHOR, &XmlParser::ParseEpigraphAuthor },
		};

		Parse(PARSERS, std::move(textValue));
	}

	void OnStartElementCoverpageImage()
	{
		m_coverpage = attributes().value(L_HREF).toString();
		while (!m_coverpage.isEmpty() && m_coverpage.front() == '#')
			m_coverpage.removeFirst();
	}

	void OnStartElementBinary()
	{
		m_covers.emplace_back(attributes().value(ID).toString(), QByteArray {});
	}

	void OnStartElementSection()
	{
		m_currentContentItem = m_currentContentItem->AppendChild(NavigationItem::Create()).get();
	}

	void OnEndElementSection()
	{
		const auto remove = m_currentContentItem->GetData(NavigationItem::Column::Title).isEmpty();
		m_currentContentItem = m_currentContentItem->GetParent();
		if (remove)
			m_currentContentItem->RemoveChild();
	}

	void ParseAnnotation(QString && value)
	{
		m_data.annotation.append(QString("<p>%1</p>").arg(value));
	}

	void ParseAnnotationHref(QString && value)
	{
		m_data.annotation.append(QString(R"(<p><a href="%1">%2</a></p>)").arg(m_href).arg(value));
	}

	void ParseKeywords(QString && value)
	{
		std::ranges::copy(value.split(",", Qt::SkipEmptyParts), std::back_inserter(m_data.keywords));
	}

	void ParseBinary(QString && value)
	{
		m_covers.back().second = QByteArray::fromBase64(value.toUtf8());
	}

	// ReSharper disable once CppMemberFunctionMayBeConst
	void ParseSectionTitle(QString && value)
	{
		if (std::ranges::all_of(value, [] (auto c) { return c.isDigit(); }))
			return;

		auto currentValue = m_currentContentItem->GetData(NavigationItem::Column::Title);
		m_currentContentItem->SetData(currentValue.append(currentValue.isEmpty() ? "" : ". ").append(value), NavigationItem::Column::Title);
	}

	void ParseEpigraph(QString && value)
	{
		m_data.epigraph = std::move(value);
	}

	void ParseEpigraphAuthor(QString && value)
	{
		m_data.epigraphAuthor = std::move(value);
	}

	template <typename Value, size_t ArraySize, typename... ARGS>
	void Parse(Value(&array)[ArraySize], ARGS &&... args)
	{
		const auto & key = m_stack.ToString();
		const auto parser = FindSecond(array, key.toStdString().data(), &XmlParser::Stub<ARGS...>, PszComparerEndsWithCaseInsensitive {});
		std::invoke(parser, *this, std::forward<ARGS>(args)...);
	}

	void update_covers(const QString & rootFolder, const IDataItem & book)
	{
		if (std::ranges::none_of(m_covers, [] (const auto & item)
		{
			return item.second.isNull();
		}))
			return;

		const auto zip = CreateImageArchive(QString("%1/%2").arg(rootFolder, book.GetRawData(BookItem::Column::Folder)));
		if (!zip)
			return;

		const auto read = [&](const QString &id)
		{
			try
			{
				return zip->Read(QString("%1/%2").arg(book.GetRawData(BookItem::Column::FileName), id)).readAll();
			}
			catch(const std::exception & ex)
			{
				PLOGE << ex.what();
			}
			return QByteArray {};
		};

		for (auto & [id, bytes] : m_covers)
			if (bytes.isNull())
				bytes = read(id);
	}

private:
	XmlStack m_stack;
	ArchiveParser::Data m_data;
	QString m_href;
	QString m_coverpage;
	IDataItem * m_currentContentItem { m_data.content.get() };
	std::vector<std::pair<QString, QByteArray>> m_covers;
};

}

class ArchiveParser::Impl
{
public:
	explicit Impl(std::shared_ptr<ICollectionController> collectionController, std::shared_ptr<ZipProgressCallback> zipProgressCallback)
		: m_collectionController(std::move(collectionController))
		, m_zipProgressCallback(std::move(zipProgressCallback))
	{
	}

	[[nodiscard]] Data Parse(const IDataItem & book) const
	{
		const auto collection = m_collectionController->GetActiveCollection();
		assert(collection);
		const auto folder = QString("%1/%2").arg(collection->folder, book.GetRawData(BookItem::Column::Folder));
		if (!QFile::exists(folder))
		{
			PLOGW << folder << " not found";
			return {};
		}

		try
		{
			const Zip zip(folder, m_zipProgressCallback);
			auto & stream = zip.Read(book.GetRawData(BookItem::Column::FileName));
			XmlParser parser(stream);
			return parser.Parse(collection->folder, book);
		}
		catch(...) {}
		return {};
	}

private:
	PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
	std::shared_ptr<ZipProgressCallback> m_zipProgressCallback;
};

ArchiveParser::ArchiveParser(std::shared_ptr<ICollectionController> collectionController, std::shared_ptr<ZipProgressCallback> zipProgressCallback)
	: m_impl(std::move(collectionController), std::move(zipProgressCallback))
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
