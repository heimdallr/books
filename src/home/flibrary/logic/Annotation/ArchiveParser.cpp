#include "ArchiveParser.h"

#include <ranges>

#include <QFile>

#include "fnd/FindPair.h"
#include "fnd/ScopedCall.h"

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

constexpr auto CONTEXT    = "Annotation";
constexpr auto CONTENT    = QT_TRANSLATE_NOOP("Annotation", "Content");
constexpr auto FILE_EMPTY = QT_TRANSLATE_NOOP("Annotation", "File is empty");
TR_DEF

constexpr auto ID                     = "id";
constexpr auto A                      = "a";
constexpr auto P                      = "p";
constexpr auto EMPHASIS               = "emphasis";
constexpr auto TRANSLATOR             = "FictionBook/description/title-info/translator";
constexpr auto TRANSLATOR_FIRST_NAME  = "FictionBook/description/title-info/translator/first-name";
constexpr auto TRANSLATOR_MIDDLE_NAME = "FictionBook/description/title-info/translator/middle-name";
constexpr auto TRANSLATOR_LAST_NAME   = "FictionBook/description/title-info/translator/last-name";
constexpr auto TRANSLATOR_NICKNAME    = "FictionBook/description/title-info/translator/nickname";
constexpr auto ANNOTATION             = "FictionBook/description/title-info/annotation";
constexpr auto KEYWORDS               = "FictionBook/description/title-info/keywords";
constexpr auto LANG                   = "FictionBook/description/title-info/lang";
constexpr auto LANG_SRC               = "FictionBook/description/title-info/src-lang";
constexpr auto BINARY                 = "FictionBook/binary";
constexpr auto COVERPAGE_IMAGE        = "FictionBook/description/title-info/coverpage/image";
constexpr auto SECTION                = "section";
constexpr auto SECTION_TITLE          = "section/title";
constexpr auto SECTION_TITLE_P        = "section/title/p";
constexpr auto SECTION_TITLE_P_STRONG = "section/title/p/strong";
constexpr auto EPIGRAPH               = "FictionBook/body/epigraph";
constexpr auto EPIGRAPH_P             = "FictionBook/body/epigraph/p";
constexpr auto EPIGRAPH_AUTHOR        = "FictionBook/body/epigraph/text-author";
constexpr auto PUBLISH_INFO_PUBLISHER = "FictionBook/description/publish-info/publisher";
constexpr auto PUBLISH_INFO_CITY      = "FictionBook/description/publish-info/city";
constexpr auto PUBLISH_INFO_YEAR      = "FictionBook/description/publish-info/year";
constexpr auto PUBLISH_INFO_ISBN      = "FictionBook/description/publish-info/isbn";
constexpr auto BODY                   = "FictionBook/body/";
constexpr auto FICTION_BOOK           = "FictionBook";

constexpr std::pair<const char*, const char*> ANNOTATION_REPLACE_TAGS[] {
	{ EMPHASIS, "em" },
	{  "style",   "" }
};

constexpr std::pair<const char*, const char*> ANNOTATION_REPLACE_ATTRIBUTE_NAME[] {
	{ "l:href", "href" },
};

QString AnnotationReplaceAttributeName(const QString& name)
{
	const auto it = std::ranges::find(ANNOTATION_REPLACE_ATTRIBUTE_NAME, name, [](const auto& item) {
		return item.first;
	});
	return it == std::end(ANNOTATION_REPLACE_ATTRIBUTE_NAME) ? name : it->second;
}

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
		if (const auto it = std::ranges::find_if(
				m_covers,
				[&](const auto& item) {
					return item.first == m_coverpage;
				}
			);
		    it != m_covers.end())
			m_data.covers.emplace_back(std::move(it->first), std::move(it->second));

		std::multimap<int, QByteArray> covers;

		ExtractBookImages(QString("%1/%2").arg(rootFolder, book.GetRawData(BookItem::Column::Folder)), book.GetRawData(BookItem::Column::FileName), [this, &covers](QString name, QByteArray data) {
			bool       ok = false;
			const auto id = name.toInt(&ok);
			if (ok)
				covers.emplace(id, std::move(data));
			else
				m_data.covers.emplace_back(std::move(name), std::move(data));

			return false;
		});

		std::ranges::transform(std::move(covers), std::back_inserter(m_data.covers), [](auto&& item) {
			return IAnnotationController::IDataProvider::Cover { QString::number(item.first), std::move(item.second) };
		});

		std::ranges::transform(
			std::move(m_covers) | std::views::filter([](const auto& item) {
				return !item.second.isNull();
			}),
			std::back_inserter(m_data.covers),
			[](auto&& item) {
				return IAnnotationController::IDataProvider::Cover { std::move(item.first), std::move(item.second) };
			}
		);

		return m_data;
	}

private: // Util::SaxParser
	bool OnStartElement(const QString& name, const QString& path, const Util::XmlAttributes& attributes) override
	{
		if (name.compare(A, Qt::CaseInsensitive) == 0)
			m_href = attributes.GetAttribute(m_hrefLink);

		if (m_annotationMode)
		{
			const auto it = std::ranges::find(ANNOTATION_REPLACE_TAGS, name, [](const auto& item) {
				return item.first;
			});
			if (const auto replacedName = it == std::end(ANNOTATION_REPLACE_TAGS) ? name : it->second; !replacedName.isEmpty())
			{
				const ScopedCall nodeGuard(
					[&] {
						m_data.annotation.append(QString("<%1").arg(replacedName));
					},
					[&] {
						m_data.annotation.append(QString(">"));
					}
				);
				for (size_t i = 0, sz = attributes.GetCount(); i < sz; ++i)
					m_data.annotation.append(QString(R"( %1="%2")").arg(AnnotationReplaceAttributeName(attributes.GetName(i)), attributes.GetValue(i)));
			}
		}

		m_textMode = path.startsWith(BODY) && (name.compare(P, Qt::CaseInsensitive) == 0 || name.compare(EMPHASIS, Qt::CaseInsensitive) == 0);

		using ParseElementFunction = bool (XmlParser::*)(const Util::XmlAttributes&);
		using ParseElementItem     = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{    FICTION_BOOK,    &XmlParser::OnStartElementFictionBook },
            { COVERPAGE_IMAGE, &XmlParser::OnStartElementCoverpageImage },
            {          BINARY,         &XmlParser::OnStartElementBinary },
			{         SECTION,        &XmlParser::OnStartElementSection },
            {      TRANSLATOR,     &XmlParser::OnStartElementTranslator },
            {      ANNOTATION,     &XmlParser::OnStartElementAnnotation },
		};

		return SaxParser::Parse(*this, PARSERS, path, attributes);
	}

	bool OnEndElement(const QString& name, const QString& path) override
	{
		const auto percents = std::lround(100 * m_ioDevice.pos() / m_total);
		if (m_percents < percents)
		{
			m_progressItem->Increment(percents - m_percents);
			m_percents = percents;
		}

		using ParseElementFunction = bool (XmlParser::*)();
		using ParseElementItem     = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{    SECTION,    &XmlParser::OnEndElementSection },
			{ ANNOTATION, &XmlParser::OnEndElementAnnotation },
		};

		const auto result = SaxParser::Parse(*this, PARSERS, path);

		if (m_annotationMode)
		{
			const auto it = std::ranges::find(ANNOTATION_REPLACE_TAGS, name, [](const auto& item) {
				return item.first;
			});
			if (const auto replacedName = it == std::end(ANNOTATION_REPLACE_TAGS) ? name : it->second; !replacedName.isEmpty())
				m_data.annotation.append(QString("</%1>").arg(replacedName));
		}

		return result;
	}

	bool OnCharacters(const QString& path, const QString& value) override
	{
		using ParseCharacterFunction = bool (XmlParser::*)(const QString&);
		using ParseCharacterItem     = std::pair<const char*, ParseCharacterFunction>;
		static constexpr ParseCharacterItem PARSERS[] {
			{             ANNOTATION,           &XmlParser::ParseAnnotation },
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
			{    TRANSLATOR_NICKNAME,   &XmlParser::ParseTranslatorNickname },
			{ PUBLISH_INFO_PUBLISHER, &XmlParser::ParsePublishInfoPublisher },
			{      PUBLISH_INFO_CITY,      &XmlParser::ParsePublishInfoCity },
			{      PUBLISH_INFO_YEAR,      &XmlParser::ParsePublishInfoYear },
			{      PUBLISH_INFO_ISBN,      &XmlParser::ParsePublishInfoIsbn },
		};

		if (m_textMode)
		{
			m_data.textSize  += value.length();
			m_data.wordCount += value.split(' ', Qt::SkipEmptyParts).size();
		}

		if (m_annotationMode)
			m_data.annotation.append(value);

		return SaxParser::Parse(*this, PARSERS, path, value);
	}

	bool OnWarning(const size_t line, const size_t column, const QString& text) override
	{
		return Log(line, column, text, plog::Severity::warning);
	}

	bool OnError(const size_t line, const size_t column, const QString& text) override
	{
		return Log(line, column, text, plog::Severity::error);
	}

	bool OnFatalError(const size_t line, const size_t column, const QString& text) override
	{
		return Log(line, column, text, plog::Severity::fatal);
	}

private:
	bool Log(const size_t line, const size_t column, const QString& text, const plog::Severity severity)
	{
		m_data.error = text;
		LOG(severity) << line << ":" << column << " " << text;
		return true;
	}

	bool OnStartElementFictionBook(const Util::XmlAttributes& attributes)
	{
		for (size_t i = 0, sz = attributes.GetCount(); i < sz; ++i)
			if (const auto attributeName = attributes.GetName(i); attributeName.startsWith("xmlns:"))
				return (m_hrefLink = attributeName.last(attributeName.length() - 6) + ":href"), true;

		return true;
	}

	bool OnStartElementCoverpageImage(const Util::XmlAttributes& attributes)
	{
		m_coverpage   = attributes.GetAttribute(m_hrefLink);
		const auto it = std::ranges::find_if(m_coverpage, [](const auto ch) {
			return ch != '#';
		});
		m_coverpage   = m_coverpage.last(std::distance(it, m_coverpage.end()));

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

	bool OnStartElementAnnotation(const Util::XmlAttributes&)
	{
		m_annotationMode = true;
		return true;
	}

	bool OnEndElementSection()
	{
		auto       title  = m_currentContentItem->GetData(NavigationItem::Column::Title).simplified();
		const auto remove = title.isEmpty();
		m_currentContentItem->SetData(std::move(title), NavigationItem::Column::Title);
		m_currentContentItem = m_currentContentItem->GetParent();
		if (remove)
			m_currentContentItem->RemoveChild();

		return true;
	}

	bool OnEndElementAnnotation()
	{
		m_annotationMode = false;
		return true;
	}

	bool ParseAnnotation(const QString& value)
	{
		m_data.annotation.append(value);
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
		if (std::ranges::all_of(value, [](auto c) {
				return c.isDigit();
			}))
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

	bool ParseTranslatorNickname(const QString& value)
	{
		return ParseTranslatorName(value, AuthorItem::Column::LastName, true);
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
	bool ParseTranslatorName(const QString& value, const int column, const bool ifEmpty = false) const
	{
		assert(m_data.translators->GetChildCount() > 0);
		const auto translator = m_data.translators->GetChild(m_data.translators->GetChildCount() - 1);
		if (!ifEmpty || translator->GetData(column).isEmpty())
			translator->SetData(value, column);
		return true;
	}

private:
	QIODevice&                                          m_ioDevice;
	int64_t                                             m_total;
	std::unique_ptr<IProgressController::IProgressItem> m_progressItem;

	ArchiveParser::Data                         m_data;
	QString                                     m_hrefLink;
	QString                                     m_href;
	QString                                     m_coverpage;
	IDataItem*                                  m_currentContentItem { m_data.content.get() };
	std::vector<std::pair<QString, QByteArray>> m_covers;
	int64_t                                     m_percents { 0 };
	bool                                        m_textMode { false };
	bool                                        m_annotationMode { false };
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
		const auto  folder     = QString("%1/%2").arg(collection.GetFolder(), book.GetRawData(BookItem::Column::Folder));
		if (!QFile::exists(folder))
		{
			PLOGW << folder << " not found";
			return {};
		}

		auto parseProgressItem = m_progressController->Add(100);

		const Zip  zip(folder, m_logicFactory->CreateZipProgressCallback(m_progressController));
		const auto stream = zip.Read(book.GetRawData(BookItem::Column::FileName));

		XmlParser parser(stream->GetStream());
		return parser.Parse(collection.GetFolder(), book, std::move(parseProgressItem));
	}

private:
	PropagateConstPtr<const ICollectionProvider, std::shared_ptr> m_collectionProvider;
	std::shared_ptr<IProgressController>                          m_progressController;
	std::shared_ptr<const ILogicFactory>                          m_logicFactory;
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
