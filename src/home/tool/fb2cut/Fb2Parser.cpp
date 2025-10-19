#include "Fb2Parser.h"

#include <QCryptographicHash>

#include <set>
#include <stack>
#include <unordered_set>

#include <QFile>
#include <QIODevice>
#include <QRegularExpression>
#include <QString>
#include <QTextStream>

#include "fnd/FindPair.h"
#include "fnd/IsOneOf.h"
#include "fnd/algorithm.h"

#include "util/xml/SaxParser.h"
#include "util/xml/XmlAttributes.h"
#include "util/xml/XmlWriter.h"

#include "log.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace fb2cut;

namespace
{

constexpr auto ID     = "id";
constexpr auto L_HREF = "l:href";

constexpr auto FICTION_BOOK    = "FictionBook";
constexpr auto BINARY          = "FictionBook/binary";
constexpr auto BODY            = "FictionBook/body";
constexpr auto BODY_BINARY     = "FictionBook/body/binary";
constexpr auto COVERPAGE_IMAGE = "FictionBook/description/title-info/coverpage/image";
constexpr auto DESCRIPTION     = "FictionBook/description";
constexpr auto TITLE           = "FictionBook/description/title-info/book-title";
constexpr auto DOCUMENT_INFO   = "FictionBook/description/document-info";
constexpr auto PROGRAM_USED    = "FictionBook/description/document-info/program-used";

constexpr auto SECTION     = "section";
constexpr auto SUBTITLE    = "title";
constexpr auto CUSTOM_INFO = "custom-info";
constexpr auto BR          = "br";

const std::unordered_set<QString> FB2_TAGS_CACHE { std::begin(Fb2Parser::FB2_TAGS), std::end(Fb2Parser::FB2_TAGS) };

const std::pair<QString, QString> REPLACE_CHAR[] {
	{  "&lt;",  "<" },
    {  "&gt;",  ">" },
    {  "amp;",  "&" },
    { "apos;",  "'" },
    { "quot;", "\"" },
};

class Fb2EncodingParserImpl final : public Util::SaxParser
{
public:
	explicit Fb2EncodingParserImpl(QIODevice& input)
		: SaxParser(input)
	{
		Parse();
	}

	QString GetEncoding() const
	{
		return m_encoding;
	}

private: // SaxParser
	bool OnXMLDecl(const QString& /*versionStr*/, const QString& encodingStr, const QString& /*standaloneStr*/, const QString& /*actualEncodingStr*/) override
	{
		m_encoding = encodingStr;
		return false;
	}

	bool OnFatalError(size_t /*line*/, size_t /*column*/, const QString& text) override
	{
		QRegularExpression rx("unable to create converter for '(.+?)' encoding");
		if (const auto match = rx.match(text); match.hasMatch())
			m_encoding = match.captured(1);

		return false;
	}

private:
	QString m_encoding;
};

class Fb2ImageParserImpl final : public Util::SaxParser
{
public:
	Fb2ImageParserImpl(QIODevice& input, Fb2ImageParser::OnBinaryFound binaryCallback)
		: SaxParser(input, 512)
		, m_binaryCallback { std::move(binaryCallback) }
	{
		Parse();
	}

private: // Util::SaxParser
	bool OnStartElement(const QString&, const QString& path, const Util::XmlAttributes& attributes) override
	{
		if (m_isBinary)
			throw std::runtime_error("bad binary");

		if (IsOneOf(path, BINARY, BODY_BINARY))
		{
			m_isBinary = true;
			m_picId    = attributes.GetAttribute(ID).trimmed();
			if (const auto it = std::ranges::find_if(
					m_picId,
					[](const auto ch) {
						return ch != '#';
					}
				);
			    it != m_picId.end())
				m_picId = m_picId.last(std::distance(it, m_picId.end())).trimmed();
			return true;
		}

		if (path == COVERPAGE_IMAGE)
		{
			for (size_t i = 0, sz = attributes.GetCount(); i < sz; ++i)
			{
				auto attributeName  = attributes.GetName(i);
				auto attributeValue = attributes.GetValue(i);
				if (attributeName.endsWith(":href"))
				{
					if (const auto it = std::ranges::find_if(
							attributeValue,
							[](const auto ch) {
								return ch != '#';
							}
						);
					    it != attributeValue.end())
						m_coverPage = attributeValue.last(std::distance(it, attributeValue.end())).trimmed();
					break;
				}
			}
			return true;
		}

		return true;
	}

	bool OnEndElement(const QString&, const QString& path) override
	{
		if (IsOneOf(path, BINARY, BODY_BINARY))
			m_isBinary = false;

		return true;
	}

	bool OnCharacters([[maybe_unused]] const QString& path, const QString& value) override
	{
		if (m_picId.isEmpty())
			return true;

		if (!m_isBinary || !IsOneOf(path, BINARY, BODY_BINARY))
			throw std::runtime_error("bad binary");

		const auto isCover = m_picId == m_coverPage;
		m_binaryCallback(std::move(m_picId), isCover, QByteArray::fromBase64(value.toUtf8()));
		m_picId = {};

		return true;
	}

private:
	Fb2ImageParser::OnBinaryFound m_binaryCallback;

	bool    m_isBinary { false };
	QString m_coverPage;
	QString m_picId;
};

class Fb2ParserImpl final : public Util::SaxParser
{
	struct Section
	{
		Section* parent { nullptr };
		int      depth { 0 };
		size_t   size { 0 };

		std::unordered_map<QString, size_t>   hist;
		QString                               hash;
		std::vector<std::unique_ptr<Section>> children;

		void CalculateHash()
		{
			hash = GetHashImpl();
			size = hist.size();
			hist.clear();
		}

	private:
		QString GetHashImpl() const
		{
			QCryptographicHash md5 { QCryptographicHash::Md5 };

			std::set<std::pair<size_t, QString>, std::greater<>> counter;
			std::ranges::transform(hist, std::inserter(counter, counter.begin()), [](const auto& item) {
				return std::make_pair(item.second, item.first);
			});

			for (int n = 1; const auto& word : counter | std::views::values)
			{
				md5.addData(word.toUtf8());
				if (++n > 10)
					break;
			}

			return QString::fromUtf8(md5.result().toHex());
		}
	};

public:
	Fb2ParserImpl(QString fileName, QIODevice& input, QIODevice& output, const std::unordered_map<QString, int>& replaceId)
		: SaxParser(input, 512)
		, m_fileName { std::move(fileName) }
		, m_replaceId { replaceId }
		, m_writer(output, Util::XmlWriter::Type::Xml, false)
	{
		Parse();
		//		assert(m_tags.empty());
		if (!m_tags.empty())
			m_writer.WriteStartElement(QString::number(output.pos()));
	}

	Fb2Parser::ParseResult GetResult()
	{
		QStringList sections;
		const auto  enumerate = [&](const Section& parent, const auto& r) -> void {
            sections << QString("%1%2\t%3").arg(QString(parent.depth, '\t')).arg(parent.hash).arg(parent.size);

            for (const auto& child : parent.children)
                r(*child, r);
		};

		m_section.CalculateHash();
		enumerate(m_section, enumerate);

		return { .title = std::move(m_title), .hashText = std::move(m_section.hash), .hashSections = std::move(sections) };
	}

private: // Util::SaxParser
	bool OnProcessingInstruction(const QString& target, const QString& data) override
	{
		m_writer.WriteProcessingInstruction(target, data);
		return true;
	}

	bool OnStartElement(const QString& name, const QString& path, const Util::XmlAttributes& attributes) override
	{
		if (name == BR)
			return true;

		if (name == CUSTOM_INFO)
			m_isCustomInfo = true;

		if (name == SECTION)
			m_currentSection = m_currentSection->children.emplace_back(std::make_unique<Section>(m_currentSection, m_currentSection->depth + 1)).get();

		if (!m_isCustomInfo && !FB2_TAGS_CACHE.contains(name.toLower()))
		{
			PLOGW << "Unexpected tag: " << name;
			m_writer.WriteCharacters(QString("<%1").arg(name));
			return true;
		}

		m_tags.push(name);

		if (path == FICTION_BOOK)
		{
			m_writer.WriteStartElement(name);
			m_writer.WriteAttribute("xmlns", "http://www.gribuser.ru/xml/fictionbook/2.0");
			m_writer.WriteAttribute("xmlns:l", "http://www.w3.org/1999/xlink");
			return true;
		}

		if (IsOneOf(path, BINARY, BODY_BINARY))
			return true;

		m_writer.WriteStartElement(name);
		for (size_t i = 0, sz = attributes.GetCount(); i < sz; ++i)
		{
			auto attributeName  = attributes.GetName(i);
			auto attributeValue = attributes.GetValue(i);
			ReplaceAttribute(attributeName, attributeValue);
			m_writer.WriteAttribute(attributeName, attributeValue);
		}

		return true;
	}

	bool OnEndElement(const QString& name, const QString& path) override
	{
		if (name == BR)
			return true;

		if (name == CUSTOM_INFO)
			m_isCustomInfo = false;

		if (name == SECTION)
		{
			m_currentSection->CalculateHash();
			m_currentSection = m_currentSection->parent;
			assert(m_currentSection);
		}

		if (!m_isCustomInfo && !FB2_TAGS_CACHE.contains(name.toLower()))
			return m_writer.WriteCharacters(">"), true;

		if (m_tags.top() != name)
			return false;

		m_tags.pop();

		if (path == DOCUMENT_INFO && !m_hasProgramUsed)
		{
			m_writer.WriteStartElement("program-used").WriteCharacters(QString("fb2cut %2").arg(PRODUCT_VERSION)).WriteEndElement();
			m_hasProgramUsed = true;
		}

		if (path == DESCRIPTION && !m_hasProgramUsed)
		{
			m_writer.WriteStartElement("document-info").WriteStartElement("program-used").WriteCharacters(QString("fb2cut %2").arg(PRODUCT_VERSION)).WriteEndElement().WriteEndElement();
			m_hasProgramUsed = true;
		}

		if (IsOneOf(path, BINARY, BODY_BINARY))
			return true;

		m_writer.WriteEndElement();

		return true;
	}

	bool OnCharacters(const QString& path, const QString& value) override
	{
		if (IsOneOf(path, BINARY, BODY_BINARY))
			return true;

		if (path == PROGRAM_USED)
		{
			m_writer.WriteCharacters(QString("%1, fb2cut %2").arg(value, PRODUCT_VERSION));
			m_hasProgramUsed = true;
			return true;
		}

		auto valueCopy = value;

		for (const auto& [before, after] : REPLACE_CHAR)
			valueCopy.replace(before, after, Qt::CaseInsensitive);

		m_writer.WriteCharacters(valueCopy);

		if (path == TITLE)
			return (m_title = valueCopy.simplified()), true;

		if (path.startsWith(BODY, Qt::CaseInsensitive) && !path.contains(SUBTITLE))
		{
			valueCopy = valueCopy.simplified().toLower();
			for (auto&& word : valueCopy.split(' ', Qt::SkipEmptyParts))
				if (word.length() > 5)
				{
					++m_section.hist[word];
					for (auto* section = m_currentSection; section; section = section->parent)
						++section->hist[word];
				}
		}

		return true;
	}

	bool OnWarning(const size_t line, const size_t column, const QString& text) override
	{
		PLOGW << m_fileName << " " << line << ":" << column << " " << text;
		return true;
	}

	bool OnError(const size_t line, const size_t column, const QString& text) override
	{
		PLOGE << m_fileName << " " << line << ":" << column << " " << text;
		return false;
	}

	bool OnFatalError(const size_t line, const size_t column, const QString& text) override
	{
		return OnError(line, column, text);
	}

private:
	void ReplaceAttribute(QString& name, QString& value) const
	{
		if (name.startsWith("xlink:"))
			name = "l:" + name.last(name.length() - 6);

		if (!name.endsWith(":href"))
			return;

		name = L_HREF;

		if (!value.startsWith('#'))
			return;

		if (const auto it = std::ranges::find_if(
				value,
				[](const auto ch) {
					return ch != '#';
				}
			);
		    it != value.end() && it != value.begin())
			value = value.last(std::distance(it, value.end()));

		const auto it = m_replaceId.find(value);
		if (it == m_replaceId.end())
			return (void)value.prepend('#');

		value = it->second == -1 ? QString { "#cover" } : QString("#%1").arg(it->second);
	}

private:
	const QString                           m_fileName;
	const std::unordered_map<QString, int>& m_replaceId;
	Util::XmlWriter                         m_writer;
	bool                                    m_hasProgramUsed { false };
	std::stack<QString>                     m_tags;
	bool                                    m_isCustomInfo { false };
	QString                                 m_title;
	Section                                 m_section;
	Section*                                m_currentSection { &m_section };
};

} // namespace

QString Fb2EncodingParser::GetEncoding(QIODevice& input)
{
	return Fb2EncodingParserImpl(input).GetEncoding();
}

bool Fb2ImageParser::Parse(QIODevice& input, OnBinaryFound binaryCallback)
{
	try
	{
		const Fb2ImageParserImpl parser(input, std::move(binaryCallback));
		return true;
	}
	catch (const std::exception& ex)
	{
		PLOGE << ex.what();
	}
	catch (...)
	{
		PLOGE << "unknown error";
	}
	return false;
}

Fb2Parser::ParseResult Fb2Parser::Parse(QString fileName, QIODevice& input, QIODevice& output, const std::unordered_map<QString, int>& replaceId)
{
	Fb2ParserImpl parser(std::move(fileName), input, output, replaceId);
	return parser.GetResult();
}
