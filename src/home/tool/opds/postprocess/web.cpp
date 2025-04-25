#include <ranges>

#include <QBuffer>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

#include "fnd/FindPair.h"
#include "fnd/ScopedCall.h"

#include "interface/constants/Localization.h"
#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IAnnotationController.h"

#include "util/ISettings.h"
#include "util/localization.h"
#include "util/xml/SaxParser.h"
#include "util/xml/XmlAttributes.h"
#include "util/xml/XmlWriter.h"

#include "root.h"

namespace HomeCompa::Opds
{

namespace
{

constexpr auto CONTEXT = "opds";
constexpr auto HOME = QT_TRANSLATE_NOOP("opds", "Home");
constexpr auto READ = QT_TRANSLATE_NOOP("opds", "Read");
constexpr auto SEARCH = QT_TRANSLATE_NOOP("opds", "Search");
TR_DEF

constexpr auto FEED = "feed";
constexpr auto FEED_ID = "feed/id";
constexpr auto FEED_TITLE = "feed/title";
constexpr auto ENTRY = "feed/entry";
constexpr auto ENTRY_TITLE = "feed/entry/title";
constexpr auto ENTRY_LINK = "feed/entry/link";
constexpr auto ENTRY_CONTENT = "feed/entry/content";
constexpr auto AUTHOR_NAME = "feed/entry/author/name";
constexpr auto AUTHOR_LINK = "feed/entry/author/uri";

constexpr auto MAX_WIDTH = 720;

using namespace Util;
using namespace Flibrary;

constexpr std::pair<const char*, const char*> CUSTOM_URL_SCHEMA[] {
	{  Loc::AUTHORS,  Loc::Authors },
    {   Loc::SERIES,   Loc::Series },
    {   Loc::GENRES,   Loc::Genres },
    { Loc::KEYWORDS, Loc::Keywords },
    {  Loc::ARCHIVE, Loc::Archives },
    {   Loc::GROUPS,   Loc::Groups },
};

QString ReduceSections(QString path)
{
	return path.replace(QRegularExpression("section(/section)+"), "section");
}

class AnnotationControllerStrategy final : public IAnnotationController::IStrategy
{
public:
	AnnotationControllerStrategy(const ISettings& settings)
		: m_starSymbol { settings.Get(Constant::Settings::STAR_SYMBOL_KEY, Constant::Settings::STAR_SYMBOL_DEFAULT) }
	{
	}

private: // IAnnotationController::IUrlGenerator
	QString GenerateUrl(const char* type, const QString& id, const QString& str) const override
	{
		if (str.isEmpty() || PszComparer {}(type, Constant::BOOK))
			return {};

		const auto* typeMapped = FindSecond(CUSTOM_URL_SCHEMA, type, nullptr, PszComparer {});
		if (!typeMapped)
			return str;

		return str.isEmpty() ? QString {} : QString("<a href=/web/%1/%2>%3</a>").arg(typeMapped, id, str);
	}

	QString GenerateStars(const int rate) const override
	{
		return QString { rate, m_starSymbol };
	}

private:
	const QChar m_starSymbol;
};

class AbstractParser : public SaxParser
{
	static std::unique_ptr<QBuffer> CreateStream()
	{
		auto stream = std::make_unique<QBuffer>();
		stream->open(QIODevice::WriteOnly);
		return stream;
	}

public:
	virtual QByteArray GetResult()
	{
		m_writer.reset();
		m_output->close();
		return m_output->buffer();
	}

protected:
	AbstractParser(QIODevice& stream, QString root)
		: SaxParser(stream)
		, m_root { std::move(root) }
	{
	}

	virtual void WriteHttpHead()
	{
		// clang-format off
		m_writer->WriteStartElement("html")
			.WriteStartElement("head")
				.WriteStartElement("style").WriteCharacters(QString("p { max-width: %1px; } %2").arg(MAX_WIDTH).arg(GetStyle())).WriteEndElement()
			.WriteEndElement()
			.WriteStartElement("body")
				.WriteStartElement("form").WriteAttribute("action", "/web/search").WriteAttribute("method", "GET")
					.WriteStartElement("p")
						.WriteStartElement("input").WriteAttribute("type", "text").WriteAttribute("id", "q").WriteAttribute("name", "q").WriteAttribute("placeholder", Tr(SEARCH)).WriteAttribute("size", "64")
						.WriteEndElement()
					.WriteEndElement()
				.WriteEndElement()
				.WriteStartElement("a").WriteAttribute("href", m_root).WriteCharacters(Tr(HOME)).WriteEndElement();
		// clang-format on
		WriteHead();
		m_writer->Guard("hr");
	}

private: // SaxParser
	bool OnWarning(const QString& /*text*/) override
	{
		return true;
	}

	bool OnError(const QString& /*text*/) override
	{
		return true;
	}

	bool OnFatalError(const QString& /*text*/) override
	{
		return true;
	}

private:
	virtual void WriteHead() = 0;

	virtual QString GetStyle() const
	{
		return {};
	}

protected:
	const QString m_root;
	std::unique_ptr<QBuffer> m_output { CreateStream() };
	PropagateConstPtr<XmlWriter> m_writer { std::make_unique<XmlWriter>(*m_output, XmlWriter::Type::Html) };
};

class ParserOpds : public AbstractParser
{
protected:
	ParserOpds(QIODevice& stream, QString root)
		: AbstractParser(stream, std::move(root))
	{
	}

protected:
	bool OnStartElement(const QString& /*name*/, const QString& path, const XmlAttributes& attributes) override
	{
		using ParseElementFunction = bool (ParserOpds::*)(const XmlAttributes&);
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{ ENTRY, &ParserOpds::OnStartElementFeedEntry },
		};

		return Parse(*this, PARSERS, path, attributes);
	}

	bool OnCharacters(const QString& path, const QString& value) override
	{
		using ParseCharacterFunction = bool (ParserOpds::*)(const QString&);
		using ParseCharacterItem = std::pair<const char*, ParseCharacterFunction>;
		static constexpr ParseCharacterItem PARSERS[] {
			{       FEED_ID,       &ParserOpds::ParseFeedId },
			{    FEED_TITLE,    &ParserOpds::ParseFeedTitle },
			{   ENTRY_TITLE,   &ParserOpds::ParseEntryTitle },
			{ ENTRY_CONTENT, &ParserOpds::ParseEntryContent },
		};

		return Parse(*this, PARSERS, path, value);
	}

	bool ParseEntryContent(const QString& value)
	{
		m_content = value;
		return true;
	}

private:
	bool ParseFeedId(const QString& value)
	{
		m_feedId = value;
		return true;
	}

	bool ParseFeedTitle(const QString& value)
	{
		m_feedTitle = value;
		return true;
	}

	bool ParseEntryTitle(const QString& value)
	{
		m_title = value;
		return true;
	}

	bool OnStartElementFeedEntry(const XmlAttributes&)
	{
		std::call_once(m_headWritten, [this] { WriteHttpHead(); });
		return true;
	}

protected: // AbstractParser
	void WriteHead() override
	{
		m_writer->Guard("h1")->WriteCharacters(m_feedTitle);
	}

protected:
	QString m_feedId;
	QString m_feedTitle;

	QString m_title;
	QString m_content;

private:
	std::once_flag m_headWritten;
};

class ParserNavigation final : public ParserOpds
{
	struct CreateGuard
	{
	};

public:
	static std::unique_ptr<AbstractParser> Create(const IPostProcessCallback&, QIODevice& stream, const QStringList& parameters)
	{
		assert(!parameters.isEmpty());
		return std::make_unique<ParserNavigation>(stream, parameters.front(), CreateGuard {});
	}

public:
	ParserNavigation(QIODevice& stream, QString root, CreateGuard)
		: ParserOpds(stream, std::move(root))
	{
	}

private: // SaxParser
	bool OnStartElement(const QString& name, const QString& path, const XmlAttributes& attributes) override
	{
		const auto result = ParserOpds::OnStartElement(name, path, attributes);
		if (m_processed)
			return result;

		using ParseElementFunction = bool (ParserNavigation::*)(const XmlAttributes&);
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{ ENTRY_LINK, &ParserNavigation::OnStartElementEntryLink },
		};

		return Parse(*this, PARSERS, path, attributes);
	}

	bool OnEndElement(const QString& /*name*/, const QString& path) override
	{
		using ParseElementFunction = bool (ParserNavigation::*)();
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{  FEED,  &ParserNavigation::OnEndElementFeed },
			{ ENTRY, &ParserNavigation::OnEndElementEntry },
		};

		return Parse(*this, PARSERS, path);
	}

private:
	void WriteHttpHead() override
	{
		AbstractParser::WriteHttpHead();
		m_tableGuard = std::make_unique<ScopedCall>([this] { m_writer->WriteStartElement("table"); }, [this] { m_writer->WriteEndElement(); });
	}

	bool OnStartElementEntryLink(const XmlAttributes& attributes)
	{
		m_link = attributes.GetAttribute("href");
		return true;
	}

	bool OnEndElementFeed()
	{
		m_tableGuard.reset();
		return true;
	}

	bool OnEndElementEntry()
	{
		auto tr = m_writer->Guard("tr");
		m_writer->Guard("td")->WriteStartElement("a").WriteAttribute("href", m_link).WriteCharacters(m_title).WriteEndElement();
		m_writer->Guard("td")->WriteCharacters(m_content);

		return true;
	}

private:
	QString m_link;
	std::unique_ptr<ScopedCall> m_tableGuard;
};

class ParserBookInfo final : public ParserOpds
{
	struct CreateGuard
	{
	};

public:
	static std::unique_ptr<AbstractParser> Create(const IPostProcessCallback& callback, QIODevice& stream, const QStringList& parameters)
	{
		assert(!parameters.isEmpty());
		return std::make_unique<ParserBookInfo>(callback, stream, parameters.front(), CreateGuard {});
	}

public:
	ParserBookInfo(const IPostProcessCallback& callback, QIODevice& stream, QString root, CreateGuard)
		: ParserOpds(stream, std::move(root))
		, m_callback { callback }
	{
	}

private: // SaxParser
	bool OnStartElement(const QString& name, const QString& path, const XmlAttributes& attributes) override
	{
		const auto result = ParserOpds::OnStartElement(name, path, attributes);
		if (m_processed)
			return result;

		using ParseElementFunction = bool (ParserBookInfo::*)(const XmlAttributes&);
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{ ENTRY_LINK, &ParserBookInfo::OnStartElementEntryLink },
		};

		return Parse(*this, PARSERS, path, attributes);
	}

	bool OnEndElement(const QString& /*name*/, const QString& path) override
	{
		using ParseElementFunction = bool (ParserBookInfo::*)();
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{ FEED, &ParserBookInfo::OnEndElementFeed },
		};

		return Parse(*this, PARSERS, path);
	}

	bool OnCharacters(const QString& path, const QString& value) override
	{
		const auto result = ParserOpds::OnCharacters(path, value);
		if (m_processed)
			return result;

		using ParseCharacterFunction = bool (ParserBookInfo::*)(const QString&);
		using ParseCharacterItem = std::pair<const char*, ParseCharacterFunction>;
		static constexpr ParseCharacterItem PARSERS[] {
			{ ENTRY_CONTENT, &ParserBookInfo::ParseEntryContent },
			{   AUTHOR_NAME,   &ParserBookInfo::ParseAuthorName },
			{   AUTHOR_LINK,   &ParserBookInfo::ParseAuthorLink },
		};

		return Parse(*this, PARSERS, path, value);
	}

private:
	bool OnStartElementEntryLink(const XmlAttributes& attributes)
	{
		const auto rel = attributes.GetAttribute("rel");
		auto href = attributes.GetAttribute("href");

		if (rel == "http://opds-spec.org/image")
			return m_coverLink = std::move(href), true;

		if (rel == "http://opds-spec.org/acquisition")
		{
			const auto type = attributes.GetAttribute("type");
			if (type == "application/fb2")
				m_downloadLinkFb2 = std::move(href);
			else if (type == "application/fb2+zip")
				m_downloadLinkZip = std::move(href);
			return true;
		}

		return true;
	}

	bool OnEndElementFeed()
	{
		AbstractParser::WriteHttpHead();
		{
			auto table = m_writer->Guard("table"), tr = m_writer->Guard("tr");
			if (!m_coverLink.isEmpty())
				m_writer->Guard("td")->Guard("img")->WriteAttribute("src", m_coverLink).WriteAttribute("width", "360");

			const auto createLink = [&](const QString& url, const QFileInfo& fileInfo, const bool isZip, const bool transliterated)
			{
				if (url.isEmpty())
					return;

				const auto fileName = isZip ? fileInfo.completeBaseName() + ".zip" : fileInfo.fileName();
				m_writer->Guard("br");
				m_writer->Guard("a")->WriteAttribute("href", QString("%1%2").arg(url, transliterated ? "/tr" : "")).WriteAttribute("download", fileName).WriteCharacters(fileName);
			};

			auto ts = m_writer->Guard("td");
			m_writer->WriteAttribute("style", "vertical-align: bottom; padding-left: 7px;").WriteCharacters("");

			m_output->write(m_content.toUtf8());
			m_writer->Guard("a")->WriteAttribute("href", QString("/web/read/%1").arg(m_feedId)).WriteCharacters(Tr(READ));

			const auto createLinks = [&](const QFileInfo& fileInfo, const bool transliterated)
			{
				m_writer->Guard("br");
				createLink(m_downloadLinkFb2, fileInfo, false, transliterated);
				createLink(m_downloadLinkZip, fileInfo, true, transliterated);
			};

			const auto fileName = m_callback.GetFileName(m_feedId, false);
			const QFileInfo fileInfo(fileName);
			createLinks(fileInfo, false);

			if (const auto fileNameTransliterated = m_callback.GetFileName(m_feedId, true); fileNameTransliterated != fileName)
				if (const QFileInfo fileInfoTransliterated(fileNameTransliterated); fileInfoTransliterated.fileName() != fileInfo.fileName())
					createLinks(fileInfoTransliterated, true);
		}

		return true;
	}

	bool ParseAuthorName(const QString& value)
	{
		m_authors.emplace_back(value, QString {});
		return true;
	}

	bool ParseAuthorLink(const QString& value)
	{
		assert(!m_authors.empty() && m_authors.back().second.isEmpty());
		m_authors.back().second = value;
		return true;
	}

private: // AbstractParser
	void WriteHttpHead() override
	{
	}

	void WriteHead() override
	{
		{
			auto h2 = m_writer->Guard("h2");
			for (const auto& [name, link] : m_authors | std::views::filter([](const auto& item) { return !item.first.isEmpty() && !item.second.isEmpty(); }))
				m_writer->Guard("a")->WriteAttribute("href", link).WriteCharacters(name);
		}
		ParserOpds::WriteHead();
	}

private:
	const IPostProcessCallback& m_callback;
	std::vector<std::pair<QString, QString>> m_authors;
	QString m_downloadLinkFb2;
	QString m_downloadLinkZip;
	QString m_coverLink;
};

class ParserFb2 final : public AbstractParser
{
	struct CreateGuard
	{
	};

	struct Link
	{
		QString href;
		QString type;
	};

	struct Binary
	{
		QString id;
		QString type;
		QString body;
	};

	static constexpr auto FICTION_BOOK = "FictionBook";
	static constexpr auto AUTHOR = "FictionBook/description/title-info/author";
	static constexpr auto AUTHOR_FIRST_NAME = "FictionBook/description/title-info/author/first-name";
	static constexpr auto AUTHOR_LAST_NAME = "FictionBook/description/title-info/author/last-name";
	static constexpr auto AUTHOR_MIDDLE_NAME = "FictionBook/description/title-info/author/middle-name";
	static constexpr auto BOOK_TITLE = "FictionBook/description/title-info/book-title";
	static constexpr auto BODY = "FictionBook/body";
	static constexpr auto BODY_TITLE = "FictionBook/body/title";
	static constexpr auto BODY_TITLE_P = "FictionBook/body/title/p";
	static constexpr auto BODY_TITLE_P_STRONG = "FictionBook/body/title/p/strong";
	static constexpr auto EPIGRAPH = "FictionBook/body/epigraph";
	static constexpr auto EPIGRAPH_P = "FictionBook/body/epigraph/p";
	static constexpr auto EPIGRAPH_TEXT_AUTHOR = "FictionBook/body/epigraph/text-author";
	static constexpr auto SECTION = "FictionBook/body/section";
	static constexpr auto SECTION_TITLE = "FictionBook/body/section/title";
	static constexpr auto SECTION_TITLE_P = "FictionBook/body/section/title/p";
	static constexpr auto SECTION_EPIGRAPH = "FictionBook/body/section/epigraph";
	static constexpr auto SECTION_EPIGRAPH_P = "FictionBook/body/section/epigraph/p";
	static constexpr auto SECTION_EPIGRAPH_TEXT_AUTHOR = "FictionBook/body/section/epigraph/text-author";
	static constexpr auto BINARY = "FictionBook/binary";

	static constexpr auto EMPTY_LINE = "empty-line";
	static constexpr auto A = "a";
	static constexpr auto POEM = "poem";
	static constexpr auto STANZA = "stanza";
	static constexpr auto P = "p";
	static constexpr auto V = "v";
	static constexpr auto NOTE = "note";
	static constexpr auto NOTES = "notes";
	static constexpr auto EMPHASIS = "emphasis";
	static constexpr auto IMAGE = "image";

public:
	static std::unique_ptr<AbstractParser> Create(const IPostProcessCallback&, QIODevice& stream, const QStringList& parameters)
	{
		assert(parameters.size() > 1);
		return std::make_unique<ParserFb2>(stream, parameters[0], parameters[1], CreateGuard {});
	}

public:
	ParserFb2(QIODevice& stream, QString root, QString bookId, CreateGuard)
		: AbstractParser(stream, std::move(root))
		, m_bookId { std::move(bookId) }
	{
	}

private: // SaxParser
	bool OnStartElement(const QString& name, const QString& pathSrc, const XmlAttributes& attributes) override
	{
		if (name == P && m_body)
			return (m_stream << "<p>"), true;

		if (name == EMPTY_LINE || name == STANZA)
			return (m_stream << "<br/>"), true;

		if (name == A)
			return (m_link = std::make_unique<Link>(attributes.GetAttribute(QString("%1:href").arg(m_linkNs)), attributes.GetAttribute("type"))), true;

		if (name == POEM)
			return (m_poem = true), true;

		if (name == EMPHASIS)
			return (m_stream << "<i>"), true;

		if (name == IMAGE && m_body)
			return (m_stream << QString(R"(<img src="#%1"/>)").arg(attributes.GetAttribute(QString("%1:href").arg(m_linkNs)).mid(1))), true;

		if (name == V && m_poem)
			return (m_stream << R"(<div class="poem">)"), true;

		const auto path = ReduceSections(pathSrc);

		using ParseElementFunction = bool (ParserFb2::*)(const XmlAttributes&);
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{ FICTION_BOOK, &ParserFb2::OnStartElementFictionBook },
            {       AUTHOR,      &ParserFb2::OnStartElementAuthor },
            {         BODY,        &ParserFb2::OnStartElementBody },
			{      SECTION,     &ParserFb2::OnStartElementSection },
            {       BINARY,      &ParserFb2::OnStartElementBinary },
		};

		return Parse(*this, PARSERS, path, attributes);
	}

	bool OnEndElement(const QString& name, const QString& pathSrc) override
	{
		if (name == P && m_body)
			return (m_stream << "</p>"), true;

		if (name == A)
			return m_link.reset(), true;

		if (name == POEM)
			return (m_stream << "<br/>\n"), (m_poem = false), true;

		if (name == EMPHASIS)
			return (m_stream << "</i>"), true;

		if (name == V && m_poem)
			return (m_stream << "</div>\n"), true;

		const auto path = ReduceSections(pathSrc);
		using ParseElementFunction = bool (ParserFb2::*)();
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{  FICTION_BOOK,  &ParserFb2::OnEndElementFictionBook },
			{        AUTHOR,       &ParserFb2::OnEndElementAuthor },
			{          BODY,         &ParserFb2::OnEndElementBody },
			{ SECTION_TITLE, &ParserFb2::OnEndElementSectionTitle },
		};

		return Parse(*this, PARSERS, path);
	}

	bool OnCharacters(const QString& pathSrc, const QString& value) override
	{
		const auto path = ReduceSections(pathSrc);
		using ParseCharacterFunction = bool (ParserFb2::*)(const QString&);
		using ParseCharacterItem = std::pair<const char*, ParseCharacterFunction>;
		static constexpr ParseCharacterItem PARSERS[] {
			{            AUTHOR_FIRST_NAME,    &ParserFb2::ParseAuthorFirstName },
			{           AUTHOR_MIDDLE_NAME,   &ParserFb2::ParseAuthorMiddleName },
			{             AUTHOR_LAST_NAME,     &ParserFb2::ParseAuthorLastName },
			{				   BOOK_TITLE,          &ParserFb2::ParseBookTitle },
			{				   BODY_TITLE,          &ParserFb2::ParseBodyTitle },
			{				 BODY_TITLE_P,          &ParserFb2::ParseBodyTitle },
			{          BODY_TITLE_P_STRONG,          &ParserFb2::ParseBodyTitle },
			{					 EPIGRAPH,           &ParserFb2::ParseEpigraph },
			{				   EPIGRAPH_P,           &ParserFb2::ParseEpigraph },
			{             SECTION_EPIGRAPH,           &ParserFb2::ParseEpigraph },
			{           SECTION_EPIGRAPH_P,           &ParserFb2::ParseEpigraph },
			{         EPIGRAPH_TEXT_AUTHOR, &ParserFb2::ParseEpigraphTextAuthor },
			{ SECTION_EPIGRAPH_TEXT_AUTHOR, &ParserFb2::ParseEpigraphTextAuthor },
			{				SECTION_TITLE,       &ParserFb2::ParseSectionTitle },
			{			  SECTION_TITLE_P,       &ParserFb2::ParseSectionTitle },
			{					   BINARY,             &ParserFb2::ParseBinary },
		};

		const auto result = Parse(*this, PARSERS, path, value);
		if (m_processed || !m_body)
			return result;

		return m_processed || !m_body ? result : ProcessUnparsedCharacters(path, value);
	}

private: // AbstractParser
	QByteArray GetResult() override
	{
		auto result = AbstractParser::GetResult();

		for (const auto& [id, type, body] : m_binary)
			result.replace(QString(R"(<img src="#%1"/>)").arg(id).toUtf8(), QString(R"(<img src="data:%1;base64, %2"/>)").arg(type, body).toUtf8());

		return result;
	}

private:
	bool OnStartElementFictionBook(const XmlAttributes& attributes)
	{
		for (size_t i = 0, sz = attributes.GetCount(); i < sz; ++i)
			if (attributes.GetValue(i) == "http://www.w3.org/1999/xlink")
			{
				m_linkNs = attributes.GetName(i).mid(6);
				break;
			}
		return true;
	}

	bool OnStartElementAuthor(const XmlAttributes&)
	{
		m_author.clear();
		m_author << "" << "" << "";
		return true;
	}

	bool OnStartElementBody(const XmlAttributes& attributes)
	{
		m_body = true;
		m_bodyName = attributes.GetAttribute("name");
		if (!m_bodyName.isEmpty())
			return true;

		WriteHttpHead();
		return true;
	}

	bool OnStartElementSection(const XmlAttributes& attributes)
	{
		m_sectionId = attributes.GetAttribute("id");
		return true;
	}

	bool OnStartElementBinary(const XmlAttributes& attributes)
	{
		m_binary.emplace_back(attributes.GetAttribute("id"), attributes.GetAttribute("content-type"));
		return true;
	}

	bool OnEndElementFictionBook()
	{
		m_stream << "</body>\n</html>\n";
		return true;
	}

	bool OnEndElementAuthor()
	{
		m_authors << m_author.join(' ').split(' ', Qt::SkipEmptyParts).join(' ');
		return true;
	}

	bool OnEndElementBody()
	{
		m_body = false;
		m_bodyName.clear();
		return true;
	}

	bool OnEndElementSectionTitle()
	{
		if (m_bodyName.isEmpty())
			return true;

		if (m_bodyName == NOTES)
		{
			m_stream << QString(R"(<a id="%1">%2</a>)").arg(m_sectionId, m_sectionTitle);
			return true;
		}

		return true;
	}

	bool ParseAuthorFirstName(const QString& value)
	{
		m_author[0] = value;
		return true;
	}

	bool ParseAuthorMiddleName(const QString& value)
	{
		m_author[1] = value;
		return true;
	}

	bool ParseAuthorLastName(const QString& value)
	{
		m_author[2] = value;
		return true;
	}

	bool ParseBookTitle(const QString& value)
	{
		m_bookTitle = value;
		return true;
	}

	bool ParseBodyTitle(const QString& value)
	{
		if (!m_bodyName.isEmpty())
			m_stream << QString("<h3>%1</h3>\n").arg(value);
		else if (m_bookTitle.isEmpty())
			m_bookTitle = value;
		return true;
	}

	bool ParseEpigraph(const QString& value)
	{
		m_stream << QString(R"(<p class="epigraph">%1</p>)"
		                    "\n")
						.arg(value);
		return true;
	}

	bool ParseEpigraphTextAuthor(const QString& value)
	{
		m_stream << QString(R"(<p class="epigraph_text_author">%1</p>)"
		                    "\n")
						.arg(value);
		return true;
	}

	bool ParseSectionTitle(const QString& value)
	{
		m_sectionTitle = value;
		if (m_bodyName.isEmpty())
			return (m_stream << QString("<h3>%1</h3>\n").arg(value)), true;

		return true;
	}

	bool ParseBinary(const QString& value)
	{
		m_binary.back().body = value;
		return true;
	}

	bool ProcessUnparsedCharacters(const QString& /*path*/, const QString& value)
	{
		if (m_link)
		{
			const auto linkGuard = m_link->type == NOTE ? std::make_unique<ScopedCall>([&] { m_stream << "<sup>"; }, [&] { m_stream << "</sup>"; }) : std::unique_ptr<ScopedCall> {};
			m_stream << QString(R"(<a href="%1">%2</a>)").arg(m_link->href, value);
			return true;
		}

		m_stream << value;
		return true;
	}

private: // AbstractParser
	void WriteHead() override
	{
		m_writer->Guard("h2")->WriteCharacters(m_authors.join(", "));
		m_writer->Guard("h1")->WriteCharacters(m_bookTitle);
	}

	QString GetStyle() const override
	{
		constexpr auto style = R"(
			.epigraph {
				margin-left: %2px;
				max-width: %2px;
				font-style: italic;
				text-align: end;
			}
			.epigraph_text_author {
				text-align: end;
			}
			.poem {
				max-width: %1px;
				text-align: center;
			}
		)";

		return QString(style).arg(MAX_WIDTH).arg(MAX_WIDTH / 2);
	}

private:
	QTextStream m_stream { m_output.get() };

	const QString m_bookId;
	QStringList m_author;
	QStringList m_authors;
	bool m_body { false };
	QString m_bodyName;
	QString m_bookTitle;
	QString m_sectionId;
	QString m_sectionTitle;
	QString m_linkNs;
	std::unique_ptr<Link> m_link;
	bool m_poem { false };
	std::vector<Binary> m_binary;
};

constexpr std::pair<ContentType, std::unique_ptr<AbstractParser> (*)(const IPostProcessCallback&, QIODevice&, const QStringList&)> PARSER_CREATORS[] {
	{ ContentType::BookInfo, &ParserBookInfo::Create },
	{ ContentType::BookText,      &ParserFb2::Create },
};

} // namespace

QByteArray PostProcess_web(const IPostProcessCallback& callback, QIODevice& stream, const ContentType contentType, const QStringList& parameters)
{
	const auto parserCreator = FindSecond(PARSER_CREATORS, contentType, &ParserNavigation::Create);
	const auto parser = parserCreator(callback, stream, parameters);
	parser->Parse();
	return parser->GetResult();
}

std::unique_ptr<IAnnotationController::IStrategy> CreateAnnotationControllerStrategy_web(const ISettings& settings)
{
	return std::make_unique<AnnotationControllerStrategy>(settings);
}

} // namespace HomeCompa::Opds
