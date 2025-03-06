#include <QBuffer>
#include <QTextStream>

#include "fnd/FindPair.h"

#include "interface/constants/Localization.h"
#include "interface/constants/ProductConstant.h"
#include "interface/logic/IAnnotationController.h"

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
TR_DEF

constexpr auto FEED = "feed";
constexpr auto FEED_TITLE = "feed/title";
constexpr auto FEED_LINK = "feed/link";
constexpr auto ENTRY = "feed/entry";
constexpr auto ENTRY_TITLE = "feed/entry/title";
constexpr auto ENTRY_LINK = "feed/entry/link";
constexpr auto ENTRY_CONTENT = "feed/entry/content";

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

class UrlGenerator final : public IAnnotationController::IUrlGenerator
{
private: // IAnnotationController::IUrlGenerator
	QString Generate(const char* type, const QString& id, const QString& str) const override
	{
		if (str.isEmpty() || PszComparer {}(type, Constant::BOOK))
			return {};

		const auto* typeMapped = FindSecond(CUSTOM_URL_SCHEMA, type, nullptr, PszComparer {});
		if (!typeMapped)
			return str;

		return str.isEmpty() ? QString {} : QString("<a href=/web/%1/%2>%3</a>").arg(typeMapped, id, str);
	}
};

class AbstractParser : public SaxParser
{
public:
	QByteArray GetResult() const
	{
		return m_output.buffer();
	}

protected:
	explicit AbstractParser(QIODevice& stream)
		: SaxParser(stream)
	{
		m_output.open(QIODevice::WriteOnly);
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
	virtual bool OnStartElementFeed(const XmlAttributes&)
	{
		m_stream << "<html>\n<body>\n";
		return true;
	}

	bool OnStartElementFeedLink(const XmlAttributes& attributes)
	{
		if (attributes.GetAttribute("rel") == "start")
			m_homeLink = QString(R"(<br><a href="%1">%2</a></br>)").arg(attributes.GetAttribute("href"), Tr(HOME));
		return true;
	}

	virtual bool OnStartElementEntry(const XmlAttributes&)
	{
		if (!m_homeLink.isEmpty())
			m_stream << m_homeLink;

		if (!m_mainTitle.isEmpty())
			m_stream << m_mainTitle;

		m_homeLink.clear();
		m_mainTitle.clear();

		m_title.clear();
		m_content.clear();
		return true;
	}

	virtual bool OnEndElementFeed()
	{
		m_stream << "</body>\n</html>\n";
		m_output.close();
		return true;
	}

	bool ParseFeedTitle(const QString& value)
	{
		m_mainTitle = QString("<h1>%1</h1><hr/>").arg(value);
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
	QBuffer m_output;
	QTextStream m_stream { &m_output };
	QString m_title;
	QString m_content;

	QString m_homeLink;
	QString m_mainTitle;
};

class ParserNavigation final : public AbstractParser
{
	struct CreateGuard
	{
	};

public:
	static std::unique_ptr<AbstractParser> Create(QIODevice& stream)
	{
		return std::make_unique<ParserNavigation>(stream, CreateGuard {});
	}

public:
	ParserNavigation(QIODevice& stream, CreateGuard)
		: AbstractParser(stream)
	{
	}

private: // SaxParser
	bool OnStartElement(const QString& /*name*/, const QString& path, const XmlAttributes& attributes) override
	{
		using ParseElementFunction = bool (ParserNavigation::*)(const XmlAttributes&);
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{       FEED,      &ParserNavigation::OnStartElementFeed },
			{      ENTRY,     &ParserNavigation::OnStartElementEntry },
			{  FEED_LINK,  &ParserNavigation::OnStartElementFeedLink },
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
			{   ENTRY_TITLE,   &ParserNavigation::ParseEntryTitle },
			{ ENTRY_CONTENT, &ParserNavigation::ParseEntryContent },
			{    FEED_TITLE,    &ParserNavigation::ParseFeedTitle },
		};

		return Parse(*this, PARSERS, path, value);
	}

private:
	bool OnStartElementFeed(const XmlAttributes& attributes) override
	{
		AbstractParser::OnStartElementFeed(attributes);
		m_stream << "<table>\n";
		return true;
	}

	bool OnStartElementEntry(const XmlAttributes& attributes) override
	{
		m_link.clear();
		return AbstractParser::OnStartElementEntry(attributes);
	}

	bool OnStartElementEntryLink(const XmlAttributes& attributes)
	{
		m_link = attributes.GetAttribute("href");
		return true;
	}

	bool OnEndElementFeed() override
	{
		m_stream << "</table>\n";
		return AbstractParser::OnEndElementFeed();
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

class ParserBookInfo final : public AbstractParser
{
	struct CreateGuard
	{
	};

public:
	static std::unique_ptr<AbstractParser> Create(QIODevice& stream)
	{
		return std::make_unique<ParserBookInfo>(stream, CreateGuard {});
	}

public:
	ParserBookInfo(QIODevice& stream, CreateGuard)
		: AbstractParser(stream)
	{
	}

private: // SaxParser
	bool OnStartElement(const QString& /*name*/, const QString& path, const XmlAttributes& attributes) override
	{
		using ParseElementFunction = bool (ParserBookInfo::*)(const XmlAttributes&);
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{       FEED,      &ParserBookInfo::OnStartElementFeed },
			{      ENTRY,     &ParserBookInfo::OnStartElementEntry },
			{  FEED_LINK,  &ParserBookInfo::OnStartElementFeedLink },
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
			{   ENTRY_TITLE,   &ParserBookInfo::ParseEntryTitle },
			{ ENTRY_CONTENT, &ParserBookInfo::ParseEntryContent },
		};

		return Parse(*this, PARSERS, path, value);
	}

private:
	bool OnEndElementFeed() override
	{
		m_stream << m_content << "\n";
		return AbstractParser::OnEndElementFeed();
	}

	bool OnStartElementEntryLink(const XmlAttributes& attributes)
	{
		if (attributes.GetAttribute("rel") == "http://opds-spec.org/image")
			m_stream << QString(R"(<img src="%1" width="240">)").arg(attributes.GetAttribute("href"));
		return true;
	}
};

constexpr std::pair<ContentType, std::unique_ptr<AbstractParser> (*)(QIODevice&)> PARSER_CREATORS[] {
	{ ContentType::BookInfo, &ParserBookInfo::Create },
};

} // namespace

QByteArray PostProcess_web(QIODevice& stream, const ContentType contentType)
{
	const auto parserCreator = FindSecond(PARSER_CREATORS, contentType, &ParserNavigation::Create);
	const auto parser = std::invoke(parserCreator, std::ref(stream));
	parser->Parse();
	return parser->GetResult();
}

std::unique_ptr<IAnnotationController::IUrlGenerator> CreateUrlGenerator_web()
{
	return std::make_unique<UrlGenerator>();
}

} // namespace HomeCompa::Opds
