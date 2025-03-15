#include "ArchiveParser.h"

#include <QFile>

#include "fnd/FindPair.h"

#include "interface/constants/Localization.h"

#include "data/DataItem.h"
#include "shared/ImageRestore.h"
#include "shared/ZipProgressCallback.h"
#include "util/xml/SaxParser.h"
#include "util/xml/XmlAttributes.h"

#include "log.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT = "Annotation";
constexpr auto CONTENT = QT_TRANSLATE_NOOP("Annotation", "Content");
constexpr auto FILE_EMPTY = QT_TRANSLATE_NOOP("Annotation", "File is empty");
TR_DEF

constexpr auto ID = "id";
constexpr auto L_HREF = "l:href";
constexpr auto A = "a";
constexpr auto P = "p";
constexpr auto EMPHASIS = "emphasis";
constexpr auto TRANSLATOR = "FictionBook/description/title-info/translator";
constexpr auto TRANSLATOR_FIRST_NAME = "FictionBook/description/title-info/translator/first-name";
constexpr auto TRANSLATOR_MIDDLE_NAME = "FictionBook/description/title-info/translator/middle-name";
constexpr auto TRANSLATOR_LAST_NAME = "FictionBook/description/title-info/translator/last-name";
constexpr auto ANNOTATION = "FictionBook/description/title-info/annotation";
constexpr auto ANNOTATION_P = "FictionBook/description/title-info/annotation/p";
constexpr auto ANNOTATION_HREF = "FictionBook/description/title-info/annotation/a";
constexpr auto ANNOTATION_HREF_P = "FictionBook/description/title-info/annotation/p/a";
constexpr auto KEYWORDS = "FictionBook/description/title-info/keywords";
constexpr auto LANG = "FictionBook/description/title-info/lang";
constexpr auto LANG_SRC = "FictionBook/description/title-info/src-lang";
constexpr auto BINARY = "FictionBook/binary";
constexpr auto COVERPAGE_IMAGE = "FictionBook/description/title-info/coverpage/image";
constexpr auto SECTION = "section";
constexpr auto SECTION_TITLE = "section/title";
constexpr auto SECTION_TITLE_P = "section/title/p";
constexpr auto SECTION_TITLE_P_STRONG = "section/title/p/strong";
constexpr auto EPIGRAPH = "FictionBook/body/epigraph";
constexpr auto EPIGRAPH_P = "FictionBook/body/epigraph/p";
constexpr auto EPIGRAPH_AUTHOR = "FictionBook/body/epigraph/text-author";
constexpr auto PUBLISH_INFO_PUBLISHER = "FictionBook/description/publish-info/publisher";
constexpr auto PUBLISH_INFO_CITY = "FictionBook/description/publish-info/city";
constexpr auto PUBLISH_INFO_YEAR = "FictionBook/description/publish-info/year";
constexpr auto PUBLISH_INFO_ISBN = "FictionBook/description/publish-info/isbn";
constexpr auto BODY = "FictionBook/body/";

class XmlParser final : public Util::SaxParser
{
public:
	explicit XmlParser(QIODevice& ioDevice)
		: SaxParser(ioDevice)
		, m_ioDevice(ioDevice)
		, m_total(m_ioDevice.size())
	{
		m_data.content->SetData(Tr(CONTENT), NavigationItem::Column::Title);
	}

	ArchiveParser::Data Parse(const QString& rootFolder, const IDataItem& book, std::unique_ptr<IProgressController::IProgressItem> progressItem)
	{
		if (m_total == 0)
		{
			m_data.error = Tr(FILE_EMPTY);
			PLOGE << m_data.error;
			return m_data;
		}

		m_progressItem = std::move(progressItem);

		SaxParser::Parse();

		const auto coverIndex = static_cast<int>(m_data.covers.size());

		if (ExtractBookImages(QString("%1/%2").arg(rootFolder, book.GetRawData(BookItem::Column::Folder)),
		                      book.GetRawData(BookItem::Column::FileName),
		                      [&covers = m_data.covers](QString name, QByteArray data)
		                      {
								  covers.emplace_back(std::move(name), std::move(data));
								  return false;
							  }))
			m_data.coverIndex = coverIndex;

		for (auto&& [name, bytes] : m_covers)
		{
			if (bytes.isNull())
				continue;

			if (name == m_coverpage)
				m_data.coverIndex = static_cast<int>(m_data.covers.size());
			m_data.covers.emplace_back(std::move(name), std::move(bytes));
		}

		return m_data;
	}

private: // Util::SaxParser
	bool OnStartElement(const QString& name, const QString& path, const Util::XmlAttributes& attributes) override
	{
		if (name.compare(A, Qt::CaseInsensitive) == 0)
			m_href = attributes.GetAttribute(L_HREF);

		m_textMode = path.startsWith(BODY) && (name.compare(P, Qt::CaseInsensitive) == 0 || name.compare(EMPHASIS, Qt::CaseInsensitive));

		using ParseElementFunction = bool (XmlParser::*)(const Util::XmlAttributes&);
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{ COVERPAGE_IMAGE, &XmlParser::OnStartElementCoverpageImage },
			{          BINARY,         &XmlParser::OnStartElementBinary },
			{         SECTION,        &XmlParser::OnStartElementSection },
			{      TRANSLATOR,     &XmlParser::OnStartElementTranslator },
			{    ANNOTATION_P,    &XmlParser::OnStartElementAnnotationP },
		};

		return SaxParser::Parse(*this, PARSERS, path, attributes);
	}

	bool OnEndElement(const QString& /*name*/, const QString& path) override
	{
		const auto percents = std::lround(100 * m_ioDevice.pos() / m_total);
		if (m_percents < percents)
		{
			m_progressItem->Increment(percents - m_percents);
			m_percents = percents;
		}

		using ParseElementFunction = bool (XmlParser::*)();
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{ SECTION, &XmlParser::OnEndElementSection },
		};

		return SaxParser::Parse(*this, PARSERS, path);
	}

	bool OnCharacters(const QString& path, const QString& value) override
	{
		using ParseCharacterFunction = bool (XmlParser::*)(const QString&);
		using ParseCharacterItem = std::pair<const char*, ParseCharacterFunction>;
		static constexpr ParseCharacterItem PARSERS[] {
			{             ANNOTATION,           &XmlParser::ParseAnnotation },
			{           ANNOTATION_P,           &XmlParser::ParseAnnotation },
			{        ANNOTATION_HREF,       &XmlParser::ParseAnnotationHref },
			{      ANNOTATION_HREF_P,       &XmlParser::ParseAnnotationHref },
			{			   KEYWORDS,             &XmlParser::ParseKeywords },
			{				   LANG,                 &XmlParser::ParseLang },
			{			   LANG_SRC,              &XmlParser::ParseSrcLang },
			{				 BINARY,               &XmlParser::ParseBinary },
			{          SECTION_TITLE,         &XmlParser::ParseSectionTitle },
			{        SECTION_TITLE_P,         &XmlParser::ParseSectionTitle },
			{ SECTION_TITLE_P_STRONG,         &XmlParser::ParseSectionTitle },
			{			   EPIGRAPH,             &XmlParser::ParseEpigraph },
			{             EPIGRAPH_P,             &XmlParser::ParseEpigraph },
			{        EPIGRAPH_AUTHOR,       &XmlParser::ParseEpigraphAuthor },
			{  TRANSLATOR_FIRST_NAME,  &XmlParser::ParseTranslatorFirstName },
			{   TRANSLATOR_LAST_NAME,   &XmlParser::ParseTranslatorLastName },
			{ TRANSLATOR_MIDDLE_NAME, &XmlParser::ParseTranslatorMiddleName },
			{ PUBLISH_INFO_PUBLISHER, &XmlParser::ParsePublishInfoPublisher },
			{      PUBLISH_INFO_CITY,      &XmlParser::ParsePublishInfoCity },
			{      PUBLISH_INFO_YEAR,      &XmlParser::ParsePublishInfoYear },
			{      PUBLISH_INFO_ISBN,      &XmlParser::ParsePublishInfoIsbn },
		};

		if (m_textMode)
			m_data.textSize += value.length();

		return SaxParser::Parse(*this, PARSERS, path, value);
	}

	bool OnWarning(const QString& text) override
	{
		return Log(text, plog::Severity::warning);
	}

	bool OnError(const QString& text) override
	{
		return Log(text, plog::Severity::error);
	}

	bool OnFatalError(const QString& text) override
	{
		return Log(text, plog::Severity::fatal);
	}

private:
	bool Log(const QString& text, const plog::Severity severity)
	{
		m_data.error = text;
		LOG(severity) << text;
		return true;
	}

	bool OnStartElementCoverpageImage(const Util::XmlAttributes& attributes)
	{
		m_coverpage = attributes.GetAttribute(L_HREF);
		while (!m_coverpage.isEmpty() && m_coverpage.front() == '#')
			m_coverpage.removeFirst();

		return true;
	}

	bool OnStartElementBinary(const Util::XmlAttributes& attributes)
	{
		m_covers.emplace_back(attributes.GetAttribute(ID), QByteArray {});
		return true;
	}

	bool OnStartElementSection(const Util::XmlAttributes&)
	{
		m_currentContentItem = m_currentContentItem->AppendChild(NavigationItem::Create()).get();
		return true;
	}

	bool OnStartElementTranslator(const Util::XmlAttributes&)
	{
		m_data.translators->AppendChild(AuthorItem::Create());
		return true;
	}

	bool OnStartElementAnnotationP(const Util::XmlAttributes&)
	{
		if (!m_data.annotation.isEmpty())
			m_data.annotation.append("<p>");
		return true;
	}

	bool OnEndElementSection()
	{
		auto title = m_currentContentItem->GetData(NavigationItem::Column::Title).simplified();
		const auto remove = title.isEmpty();
		m_currentContentItem->SetData(std::move(title), NavigationItem::Column::Title);
		m_currentContentItem = m_currentContentItem->GetParent();
		if (remove)
			m_currentContentItem->RemoveChild();

		return true;
	}

	bool ParseAnnotation(const QString& value)
	{
		m_data.annotation.append(value);
		return true;
	}

	bool ParseAnnotationHref(const QString& value)
	{
		m_data.annotation.append(QString(R"(<p><a href="%1">%2</a></p>)").arg(m_href).arg(value));
		return true;
	}

	bool ParseKeywords(const QString& value)
	{
		std::ranges::copy(value.split(",", Qt::SkipEmptyParts), std::back_inserter(m_data.keywords));
		return true;
	}

	bool ParseLang(const QString& value)
	{
		m_data.language = value;
		return true;
	}

	bool ParseSrcLang(const QString& value)
	{
		m_data.sourceLanguage = value;
		return true;
	}

	bool ParseBinary(const QString& value)
	{
		m_covers.back().second = QByteArray::fromBase64(value.toUtf8());
		return true;
	}

	bool ParseSectionTitle(const QString& value)
	{
		if (std::ranges::all_of(value, [](auto c) { return c.isDigit(); }))
			return true;

		auto currentValue = m_currentContentItem->GetData(NavigationItem::Column::Title);
		m_currentContentItem->SetData(currentValue.append(currentValue.isEmpty() ? "" : ". ").append(value), NavigationItem::Column::Title);
		return true;
	}

	bool ParseEpigraph(const QString& value)
	{
		m_data.epigraph = value;
		return true;
	}

	bool ParseEpigraphAuthor(const QString& value)
	{
		m_data.epigraphAuthor = value;
		return true;
	}

	bool ParseTranslatorFirstName(const QString& value)
	{
		return ParseTranslatorName(value, AuthorItem::Column::FirstName);
	}

	bool ParseTranslatorLastName(const QString& value)
	{
		return ParseTranslatorName(value, AuthorItem::Column::LastName);
	}

	bool ParseTranslatorMiddleName(const QString& value)
	{
		return ParseTranslatorName(value, AuthorItem::Column::MiddleName);
	}

	bool ParsePublishInfoPublisher(const QString& value)
	{
		m_data.publishInfo.publisher = value;
		return true;
	}

	bool ParsePublishInfoCity(const QString& value)
	{
		m_data.publishInfo.city = value;
		return true;
	}

	bool ParsePublishInfoYear(const QString& value)
	{
		m_data.publishInfo.year = value;
		return true;
	}

	bool ParsePublishInfoIsbn(const QString& value)
	{
		m_data.publishInfo.isbn = value;
		return true;
	}

private:
	bool ParseTranslatorName(const QString& value, const int column) const
	{
		assert(m_data.translators->GetChildCount() > 0);
		const auto translator = m_data.translators->GetChild(m_data.translators->GetChildCount() - 1);
		translator->SetData(value, column);
		return true;
	}

private:
	QIODevice& m_ioDevice;
	int64_t m_total;
	std::unique_ptr<IProgressController::IProgressItem> m_progressItem;

	ArchiveParser::Data m_data;
	QString m_href;
	QString m_coverpage;
	IDataItem* m_currentContentItem { m_data.content.get() };
	std::vector<std::pair<QString, QByteArray>> m_covers;
	int64_t m_percents { 0 };
	bool m_textMode { false };
};

} // namespace

class ArchiveParser::Impl
{
public:
	explicit Impl(std::shared_ptr<const ICollectionProvider> collectionProvider, std::shared_ptr<IProgressController> progressController, std::shared_ptr<const ILogicFactory> logicFactory)
		: m_collectionProvider { std::move(collectionProvider) }
		, m_progressController { std::move(progressController) }
		, m_logicFactory { std::move(logicFactory) }
	{
	}

	[[nodiscard]] std::shared_ptr<IProgressController> GetProgressController() const
	{
		return m_progressController;
	}

	[[nodiscard]] Data Parse(const IDataItem& book) const
	{
		try
		{
			assert(m_collectionProvider->ActiveCollectionExists());

			auto data = ParseFb2(book);

			return data;
		}
		catch (const std::exception& ex)
		{
			PLOGE << ex.what();
		}
		catch (...)
		{
			PLOGE << "Unknown error";
		}
		return {};
	}

	void Stop() const
	{
		m_progressController->Stop();
	}

private:
	Data ParseFb2(const IDataItem& book) const
	{
		const auto& collection = m_collectionProvider->GetActiveCollection();
		const auto folder = QString("%1/%2").arg(collection.folder, book.GetRawData(BookItem::Column::Folder));
		if (!QFile::exists(folder))
		{
			PLOGW << folder << " not found";
			return {};
		}

		auto parseProgressItem = m_progressController->Add(100);

		const Zip zip(folder, m_logicFactory->CreateZipProgressCallback(m_progressController));
		const auto stream = zip.Read(book.GetRawData(BookItem::Column::FileName));

		XmlParser parser(stream->GetStream());
		return parser.Parse(collection.folder, book, std::move(parseProgressItem));
	}

private:
	PropagateConstPtr<const ICollectionProvider, std::shared_ptr> m_collectionProvider;
	std::shared_ptr<IProgressController> m_progressController;
	std::shared_ptr<const ILogicFactory> m_logicFactory;
};

ArchiveParser::ArchiveParser(std::shared_ptr<ICollectionProvider> collectionProvider, std::shared_ptr<IAnnotationProgressController> progressController, std::shared_ptr<const ILogicFactory> logicFactory)
	: m_impl(std::move(collectionProvider), std::move(progressController), std::move(logicFactory))
{
	PLOGV << "AnnotationParser created";
}

ArchiveParser::~ArchiveParser()
{
	PLOGV << "AnnotationParser destroyed";
}

std::shared_ptr<IProgressController> ArchiveParser::GetProgressController() const
{
	return m_impl->GetProgressController();
}

ArchiveParser::Data ArchiveParser::Parse(const IDataItem& dataItem) const
{
	return m_impl->Parse(dataItem);
}

void ArchiveParser::Stop() const
{
	m_impl->Stop();
}
