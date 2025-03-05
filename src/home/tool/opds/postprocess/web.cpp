#include <QBuffer>
#include <QTextStream>

#include "util/localization.h"
#include "util/xml/SaxParser.h"
#include "util/xml/XmlAttributes.h"

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

class Parser final : public SaxParser
{
public:
	explicit Parser(QIODevice& stream)
		: SaxParser(stream)
	{
		m_output.open(QIODevice::WriteOnly);
	}

	QByteArray GetResult() const
	{
		return m_output.buffer();
	}

private: // SaxParser
	bool OnStartElement(const QString& /*name*/, const QString& path, const XmlAttributes& attributes) override
	{
		using ParseElementFunction = bool (Parser::*)(const XmlAttributes&);
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{       FEED,      &Parser::OnStartElementFeed },
			{      ENTRY,     &Parser::OnStartElementEntry },
			{  FEED_LINK,  &Parser::OnStartElementFeedLink },
			{ ENTRY_LINK, &Parser::OnStartElementEntryLink },
		};

		return Parse(*this, PARSERS, path, attributes);
	}

	bool OnEndElement(const QString& /*name*/, const QString& path) override
	{
		using ParseElementFunction = bool (Parser::*)();
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{  FEED,  &Parser::OnEndElementFeed },
			{ ENTRY, &Parser::OnEndElementEntry },
		};

		return Parse(*this, PARSERS, path);
	}

	bool OnCharacters(const QString& path, const QString& value) override
	{
		using ParseCharacterFunction = bool (Parser::*)(const QString&);
		using ParseCharacterItem = std::pair<const char*, ParseCharacterFunction>;
		static constexpr ParseCharacterItem PARSERS[] {
			{   ENTRY_TITLE,   &Parser::ParseEntryTitle },
			{ ENTRY_CONTENT, &Parser::ParseEntryContent },
			{    FEED_TITLE,    &Parser::ParseFeedTitle },
		};

		return Parse(*this, PARSERS, path, value);
	}

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
	bool OnStartElementFeed(const XmlAttributes&)
	{
		m_stream << "<html>\n<body>\n<table>\n";
		return true;
	}

	bool OnStartElementEntry(const XmlAttributes&)
	{
		m_title.clear();
		m_link.clear();
		m_content.clear();
		return true;
	}

	bool OnStartElementFeedLink(const XmlAttributes& attributes)
	{
		if (attributes.GetAttribute("rel") == "start")
			m_stream << QString(R"(<br><a href="%1">%2</a></br><hr/>)").arg(attributes.GetAttribute("href"), Tr(HOME));
		return true;
	}

	bool OnStartElementEntryLink(const XmlAttributes& attributes)
	{
		m_link = attributes.GetAttribute("href");
		return true;
	}

	bool OnEndElementFeed()
	{
		m_stream << "</table>\n</body>\n</html>\n";
		m_output.close();
		return true;
	}

	bool OnEndElementEntry()
	{
		m_stream << QString(R"(<tr><td><a href="%1">%2</a></td><td>%3</td></tr>)"
		                    "\n")
						.arg(m_link, m_title, m_content);
		return true;
	}

	bool ParseFeedTitle(const QString& value)
	{
		m_stream << QString("<h1>%1</h1>").arg(value);
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

private:
	QBuffer m_output;
	QTextStream m_stream { &m_output };
	QString m_title;
	QString m_link;
	QString m_content;
};

} // namespace

QByteArray PostProcess_web(QIODevice& stream)
{
	//	XMLPlatformInitializer xmlPlatformInitializer;
	Parser parser(stream);
	parser.Parse();
	return parser.GetResult();
}

} // namespace HomeCompa::Opds
