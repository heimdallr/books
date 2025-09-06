#include "Fb2Parser.h"

#include <set>
#include <stack>
#include <unordered_set>

#include <QIODevice>
#include <QString>
#include <QTextStream>

#include "fnd/FindPair.h"
#include "fnd/IsOneOf.h"

#include "util/xml/SaxParser.h"
#include "util/xml/XmlAttributes.h"
#include "util/xml/XmlWriter.h"

#include "log.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace fb2cut;

namespace
{

constexpr auto ID = "id";
constexpr auto L_HREF = "l:href";

constexpr auto FICTION_BOOK = "FictionBook";
constexpr auto BINARY = "FictionBook/binary";
constexpr auto BODY_BINARY = "FictionBook/body/binary";
constexpr auto COVERPAGE_IMAGE = "FictionBook/description/title-info/coverpage/image";
constexpr auto DESCRIPTION = "FictionBook/description";
constexpr auto DOCUMENT_INFO = "FictionBook/description/document-info";
constexpr auto PROGRAM_USED = "FictionBook/description/document-info/program-used";

const std::unordered_set<QString> FB2_TAGS_CACHE { std::begin(Fb2Parser::FB2_TAGS), std::end(Fb2Parser::FB2_TAGS) };

}

class Fb2ImageParser::Impl final : public Util::SaxParser
{
public:
	explicit Impl(QIODevice& input, OnBinaryFound binaryCallback)
		: SaxParser(input, 512)
		, m_binaryCallback { std::move(binaryCallback) }
	{
		Parse();
	}

private: // Util::SaxParser
	bool OnStartElement(const QString&, const QString& path, const Util::XmlAttributes& attributes) override
	{
		if (IsOneOf(path, BINARY, BODY_BINARY))
		{
			m_picId = attributes.GetAttribute(ID).trimmed();
			if (const auto it = std::ranges::find_if(m_picId, [](const auto ch) { return ch != '#'; }); it != m_picId.end())
				m_picId = m_picId.last(std::distance(it, m_picId.end())).trimmed();
			return true;
		}

		if (path == COVERPAGE_IMAGE)
		{
			for (size_t i = 0, sz = attributes.GetCount(); i < sz; ++i)
			{
				auto attributeName = attributes.GetName(i);
				auto attributeValue = attributes.GetValue(i);
				if (attributeName.endsWith(":href"))
				{
					if (const auto it = std::ranges::find_if(attributeValue, [](const auto ch) { return ch != '#'; }); it != attributeValue.end())
						m_coverPage = attributeValue.last(std::distance(it, attributeValue.end())).trimmed();
					break;
				}
			}
			return true;
		}

		return true;
	}

	bool OnCharacters([[maybe_unused]] const QString& path, const QString& value) override
	{
		if (m_picId.isEmpty())
			return true;

		assert(IsOneOf(path, BINARY, BODY_BINARY));

		const auto isCover = m_picId == m_coverPage;
		m_binaryCallback(std::move(m_picId), isCover, QByteArray::fromBase64(value.toUtf8()));
		m_picId = {};

		return true;
	}

private:
	OnBinaryFound m_binaryCallback;
	QString m_coverPage;
	QString m_picId;
};

void Fb2ImageParser::Parse(QIODevice& input, OnBinaryFound binaryCallback)
{
	const Fb2ImageParser parser(input, std::move(binaryCallback));
}

Fb2ImageParser::Fb2ImageParser(QIODevice& input, OnBinaryFound binaryCallback)
	: m_impl(input, std::move(binaryCallback))
{
}

Fb2ImageParser::~Fb2ImageParser() = default;

class Fb2Parser::Impl final : public Util::SaxParser
{
public:
	Impl(QString fileName, QIODevice& input, QIODevice& output, const std::unordered_map<QString, int>& replaceId)
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

private: // Util::SaxParser
	bool OnProcessingInstruction(const QString& target, const QString& data) override
	{
		m_writer.WriteProcessingInstruction(target, data);
		return true;
	}

	bool OnStartElement(const QString& name, const QString& path, const Util::XmlAttributes& attributes) override
	{
		if (!FB2_TAGS_CACHE.contains(name.toLower()))
			return m_writer.WriteCharacters(QString("&lt;%1").arg(name)), true;

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
			auto attributeName = attributes.GetName(i);
			auto attributeValue = attributes.GetValue(i);
			ReplaceAttribute(attributeName, attributeValue);
			m_writer.WriteAttribute(attributeName, attributeValue);
		}

		return true;
	}

	bool OnEndElement(const QString& name, const QString& path) override
	{
		if (!FB2_TAGS_CACHE.contains(name.toLower()))
			return m_writer.WriteCharacters("&gt;"), true;

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

		m_writer.WriteCharacters(value);

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

		if (const auto it = std::ranges::find_if(value, [](const auto ch) { return ch != '#'; }); it != value.end() && it != value.begin())
			value = value.last(std::distance(it, value.end()));

		const auto it = m_replaceId.find(value);
		if (it == m_replaceId.end())
			return (void)value.prepend('#');

		value = it->second == -1 ? QString { "#cover" } : QString("#%1").arg(it->second);
	}

	bool OnStartElementBinary(const Util::XmlAttributes& attributes)
	{
		m_binaryId = attributes.GetAttribute(ID);
		return true;
	}

private:
	const QString m_fileName;
	const std::unordered_map<QString, int>& m_replaceId;
	Util::XmlWriter m_writer;
	QString m_binaryId;
	QString m_coverpage;
	std::unordered_map<QString, size_t, std::hash<QString>, std::equal_to<>> m_imageNames;
	bool m_hasProgramUsed { false };
	std::stack<QString> m_tags;
};

void Fb2Parser::Parse(QString fileName, QIODevice& input, QIODevice& output, const std::unordered_map<QString, int>& replaceId)
{
	const Fb2Parser parser(std::move(fileName), input, output, replaceId);
}

Fb2Parser::Fb2Parser(QString fileName, QIODevice& input, QIODevice& output, const std::unordered_map<QString, int>& replaceId)
	: m_impl(std::move(fileName), input, output, replaceId)
{
}

Fb2Parser::~Fb2Parser() = default;
