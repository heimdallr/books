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
constexpr auto FEED_TITLE = "feed/title";
constexpr auto ENTRY = "feed/entry";
constexpr auto ENTRY_ID = "feed/entry/id";
constexpr auto ENTRY_TITLE = "feed/entry/title";
constexpr auto ENTRY_LINK = "feed/entry/link";
constexpr auto ENTRY_CONTENT = "feed/entry/content";
constexpr auto AUTHOR_NAME = "feed/entry/author/name";
constexpr auto AUTHOR_LINK = "feed/entry/author/uri";

constexpr auto HTTP_HEAD = R"(<!DOCTYPE html>
<html>
	<head>
		<style>
			p { 
				max-width: %2px;
			}
			%5
		</style>
	</head>
	<body>
	<form action="/web/search" method="GET">
        <p><input type="text" id="q" name="q" placeholder="%6" size="64"/></p>
    </form>
	<a href="%3">%4</a>
	%1
	<hr/>
)";

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
public:
	virtual QByteArray GetResult() const
	{
		return m_output.buffer();
	}

protected:
	AbstractParser(QIODevice& stream, QString root)
		: SaxParser(stream)
		, m_root { std::move(root) }
	{
		m_output.open(QIODevice::WriteOnly);
	}

	void WriteHttpHead(const QString& head, const QString& style = {})
	{
		std::call_once(m_headWritten, [&] { m_stream << QString(HTTP_HEAD).arg(head).arg(MAX_WIDTH).arg(m_root).arg(Tr(HOME)).arg(style).arg(Tr(SEARCH)); });
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

protected:
	const QString m_root;
	QBuffer m_output;
	QTextStream m_stream { &m_output };

private:
	std::once_flag m_headWritten;
};

class ParserOpds : public AbstractParser
{
protected:
	ParserOpds(QIODevice& stream, QString root)
		: AbstractParser(stream, std::move(root))
	{
	}

protected:
	virtual bool OnStartElementEntry(const XmlAttributes&)
	{
		if (!m_mainTitle.isEmpty())
			WriteHttpHead(QString("<h1>%1</h1>").arg(m_mainTitle));

		m_mainTitle.clear();
		m_title.clear();
		m_content.clear();
		return true;
	}

	virtual bool OnEndElementFeed()
	{
		if (!m_mainTitle.isEmpty())
			WriteHttpHead(QString("<h1>%1</h1>").arg(m_mainTitle));

		m_stream << "</body>\n</html>\n";
		m_output.close();
		return true;
	}

	bool ParseEntryId(const QString& value)
	{
		m_id = value;
		return true;
	}

	bool ParseFeedTitle(const QString& value)
	{
		m_mainTitle = value;
		return true;
	}

	bool ParseEntryTitle(const QString& value)
	{
		m_title = value;
		return true;
	}

	bool ParseEntryContent(const QString& value)
	{
		m_content = value;
		return true;
	}

protected:
	QString m_id;
	QString m_title;
	QString m_content;

	QString m_mainTitle;
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
	bool OnStartElement(const QString& /*name*/, const QString& path, const XmlAttributes& attributes) override
	{
		using ParseElementFunction = bool (ParserNavigation::*)(const XmlAttributes&);
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{      ENTRY,     &ParserNavigation::OnStartElementEntry },
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

	bool OnCharacters(const QString& path, const QString& value) override
	{
		using ParseCharacterFunction = bool (ParserNavigation::*)(const QString&);
		using ParseCharacterItem = std::pair<const char*, ParseCharacterFunction>;
		static constexpr ParseCharacterItem PARSERS[] {
			{      ENTRY_ID,      &ParserNavigation::ParseEntryId },
			{   ENTRY_TITLE,   &ParserNavigation::ParseEntryTitle },
			{ ENTRY_CONTENT, &ParserNavigation::ParseEntryContent },
			{    FEED_TITLE,    &ParserNavigation::ParseFeedTitle },
		};

		return Parse(*this, PARSERS, path, value);
	}

private:
	bool OnStartElementEntry(const XmlAttributes& attributes) override
	{
		m_link.clear();
		bool needStartTable = !m_mainTitle.isEmpty();
		ParserOpds::OnStartElementEntry(attributes);
		if (needStartTable)
			m_stream << "<table>\n";
		return true;
	}

	bool OnStartElementEntryLink(const XmlAttributes& attributes)
	{
		m_link = attributes.GetAttribute("href");
		return true;
	}

	bool OnEndElementFeed() override
	{
		m_stream << "</table>\n";
		return ParserOpds::OnEndElementFeed();
	}

	bool OnEndElementEntry()
	{
		m_stream << QString(R"(<tr><td><a href="%1">%2</a></td><td>%3</td></tr>)"
		                    "\n")
						.arg(m_link, m_title, m_content);
		return true;
	}

private:
	QString m_link;
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
	bool OnStartElement(const QString& /*name*/, const QString& path, const XmlAttributes& attributes) override
	{
		using ParseElementFunction = bool (ParserBookInfo::*)(const XmlAttributes&);
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{      ENTRY,     &ParserBookInfo::OnStartElementEntry },
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
		using ParseCharacterFunction = bool (ParserBookInfo::*)(const QString&);
		using ParseCharacterItem = std::pair<const char*, ParseCharacterFunction>;
		static constexpr ParseCharacterItem PARSERS[] {
			{    FEED_TITLE,    &ParserBookInfo::ParseFeedTitle },
            {      ENTRY_ID,      &ParserBookInfo::ParseEntryId },
            {   ENTRY_TITLE,   &ParserBookInfo::ParseEntryTitle },
			{ ENTRY_CONTENT, &ParserBookInfo::ParseEntryContent },
            {   AUTHOR_NAME,   &ParserBookInfo::ParseAuthorName },
            {   AUTHOR_LINK,   &ParserBookInfo::ParseAuthorLink },
		};

		return Parse(*this, PARSERS, path, value);
	}

private:
	bool OnStartElementEntry(const XmlAttributes&) override
	{
		m_title.clear();
		m_content.clear();
		return true;
	}

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

	bool OnEndElementFeed() override
	{
		QStringList head;
		{
			QStringList authors;
			std::ranges::transform(m_authors | std::views::filter([](const auto& item) { return !item.first.isEmpty() && !item.second.isEmpty(); }),
			                       std::back_inserter(authors),
			                       [](const auto& item) { return QString(R"(<a href = "%1">%2</a>)").arg(item.second, item.first); });
			if (!authors.isEmpty())
				head << QString(R"(<h2>%1</h2>)").arg(authors.join("&ensp;"));
		}

		if (!m_mainTitle.isEmpty())
			head << QString("<h1>%1</h1>\n").arg(m_mainTitle);

		WriteHttpHead(head.join("\n"));

		{
			const ScopedCall tableGuard([this] { m_stream << "<table><tr>"; }, [this] { m_stream << "</tr></table>"; });
			if (!m_coverLink.isEmpty())
				m_stream << QString(R"(<td><img src="%1" width="480"/></td>)").arg(m_coverLink);

			const auto href = [&](const QString& url, const QFileInfo& fileInfo, const bool isZip, const bool transliterated)
			{
				if (url.isEmpty())
					return;

				const auto fileName = isZip ? fileInfo.completeBaseName() + ".zip" : fileInfo.fileName();
				m_stream << QString(R"(<br/><a href="%1%3" download="%2">%2</a>)").arg(url, fileName, transliterated ? "/tr" : "");
			};

			const ScopedCall linkGuard([this] { m_stream << R"(<td style="vertical-align: bottom; padding-left: 7px;">)"; }, [this] { m_stream << "</td>"; });
			m_stream << m_content << "\n";
			m_stream << QString(R"(<a href="/web/read/%1">%2</a>)").arg(m_id, Tr(READ));

			const auto createHrefs = [&](const QFileInfo& fileInfo, const bool transliterated)
			{
				m_stream << QString("<br/>");
				href(m_downloadLinkFb2, fileInfo, false, transliterated);
				href(m_downloadLinkZip, fileInfo, true, transliterated);
			};

			const auto fileName = m_callback.GetFileName(m_id, false);
			const QFileInfo fileInfo(fileName);
			createHrefs(fileInfo, false);

			if (const auto fileNameTransliterated = m_callback.GetFileName(m_id, true); fileNameTransliterated != fileName)
				if (const QFileInfo fileInfoTransliterated(fileNameTransliterated); fileInfoTransliterated.fileName() != fileInfo.fileName())
					createHrefs(fileInfoTransliterated, true);
		}

		return ParserOpds::OnEndElementFeed();
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
	QByteArray GetResult() const override
	{
		auto result = m_output.buffer();

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

		const auto head = QString("<h2>%1</h2>\n<h1>%2</h1>\n").arg(m_authors.join(", "), m_bookTitle);
		WriteHttpHead(head, QString(style).arg(MAX_WIDTH).arg(MAX_WIDTH / 2));
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
		m_output.close();
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

private:
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
