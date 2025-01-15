#include "XmlWriter.h"

#include <set>
#include <stack>

#include <xercesc/framework/XMLFormatter.hpp>
#include <xercesc/util/XMLUniDefs.hpp>

#include <QIODevice>

#include "XmlAttributes.h"

using namespace HomeCompa::Util;
using namespace xercesc_3_2;

namespace {

// </
constexpr XMLCh gEndElement[] = { chOpenAngle, chForwardSlash, chNull };
// <?
constexpr XMLCh gStartPI[] = { chOpenAngle, chQuestion, chNull };
// ?>
constexpr XMLCh gEndPI[] = { chQuestion, chCloseAngle, chNull };
// <?xml version="1.0" encoding="
constexpr XMLCh gXMLDecl1[] = { chOpenAngle, chQuestion, chLatin_x, chLatin_m, chLatin_l, chSpace, chLatin_v, chLatin_e, chLatin_r, chLatin_s, chLatin_i, chLatin_o, chLatin_n, chEqual, chDoubleQuote, chDigit_1, chPeriod, chDigit_0, chDoubleQuote, chSpace, chLatin_e, chLatin_n, chLatin_c, chLatin_o, chLatin_d, chLatin_i, chLatin_n, chLatin_g, chEqual, chDoubleQuote, chNull };
// "?>
constexpr XMLCh gXMLDecl2[] = { chDoubleQuote, chQuestion, chCloseAngle, chNull };

}

class XmlWriter::Impl final
	: public XMLFormatTarget
{
	NON_COPY_MOVABLE(Impl)

public:
	explicit Impl(QIODevice & stream)
		: m_stream(stream)
		, m_formatter("utf-8", this, XMLFormatter::NoEscapes, XMLFormatter::UnRep_CharRef)
	{
		m_formatter << gXMLDecl1 << m_formatter.getEncodingName() << gXMLDecl2;
	}

	~Impl() override
	{
		m_formatter << chLF;
	}

	void WriteProcessingInstruction(const QString & target, const QString & data)
	{
		m_formatter << chLF << XMLFormatter::NoEscapes << gStartPI << target.toStdU16String().data();
		if (!data.isEmpty())
			m_formatter << chSpace << data.toStdU16String().data();

		m_formatter << XMLFormatter::NoEscapes << gEndPI;
	}

	void WriteStartElement(const QString & name)
	{
		CloseTag();
		BreakLine(name);
		m_elements.emplace(name);

		m_formatter << XMLFormatter::NoEscapes << chOpenAngle << name.toStdU16String().data();

		m_tagOpened = true;
	}

	void WriteStartElement(const QString & name, const XmlAttributes & attributes)
	{
		CloseTag();
		BreakLine(name);
		m_elements.emplace(name);

		m_formatter << XMLFormatter::NoEscapes << chOpenAngle << name.toStdU16String().data();

		for (size_t i = 0, attributeCount = attributes.GetCount(); i < attributeCount; ++i)
			m_formatter
				<< XMLFormatter::NoEscapes
				<< chSpace << attributes.GetName(i).toStdU16String().data()
				<< chEqual << chDoubleQuote
				<< XMLFormatter::AttrEscapes
				<< attributes.GetValue(i).toStdU16String().data()
				<< XMLFormatter::NoEscapes
				<< chDoubleQuote
				;

		m_tagOpened = true;
	}

	void WriteEndElement()
	{
		assert(!m_elements.empty());
		const auto name = std::move(m_elements.top());
		m_elements.pop();
		if (m_tagOpened)
		{
			m_formatter << XMLFormatter::NoEscapes << chForwardSlash << chCloseAngle;
			m_tagOpened = false;
		}
		else
		{
			if (name != m_lastElement)
				BreakLine(name);
			m_formatter << XMLFormatter::NoEscapes << gEndElement << name.toStdU16String().data() << chCloseAngle;
		}
	}

	void WriteAttribute(const QString & name, const QString & value)
	{
		m_formatter
			<< XMLFormatter::NoEscapes
			<< chSpace << name.toStdU16String().data()
			<< chEqual << chDoubleQuote
			<< XMLFormatter::AttrEscapes
			<< value.toStdU16String().data()
			<< XMLFormatter::NoEscapes
			<< chDoubleQuote
			;
	}

	void WriteCharacters(const QString & data)
	{
		CloseTag();
		const auto chars = data.toStdU16String();
		m_formatter.formatBuf(chars.data(), chars.length(), XMLFormatter::CharEscapes);
	}

private: // XMLFormatTarget
	void writeChars(const XMLByte * const toWrite, const XMLSize_t count, XMLFormatter * const) override
	{
		m_stream.write(reinterpret_cast<const char *>(toWrite), static_cast<qint64>(count));
	}

private:
	void BreakLine(const QString & name)
	{
		if (m_unbreakableTags.contains(name))
			return;

		m_lastElement = name;

		m_formatter << chLF;
		for (size_t i = 0, sz = m_elements.size(); i < sz; ++i)
			m_formatter << chHTab;
	}

	void CloseTag()
	{
		if (!m_tagOpened)
			return;

		m_formatter << XMLFormatter::NoEscapes << chCloseAngle;
		m_tagOpened = false;

	}

private:
	QIODevice & m_stream;
	XMLFormatter m_formatter;
	std::stack<QString> m_elements;
	bool m_tagOpened { false };
	QString m_lastElement;

	std::set<QString> m_unbreakableTags{ "a", "emphasis", "strong", "sub", "sup", "strikethrough", "code", "image" };
};

XmlWriter::XmlWriter(QIODevice & stream)
	: m_impl(stream)
{
}

XmlWriter::~XmlWriter() = default;

XmlWriter & XmlWriter::WriteProcessingInstruction(const QString & target, const QString & data)
{
	m_impl->WriteProcessingInstruction(target, data);
	return *this;
}

XmlWriter & XmlWriter::WriteStartElement(const QString & name)
{
	m_impl->WriteStartElement(name);
	return *this;
}

XmlWriter & XmlWriter::WriteStartElement(const QString & name, const XmlAttributes & attributes)
{
	m_impl->WriteStartElement(name, attributes);
	return *this;
}

XmlWriter & XmlWriter::WriteEndElement()
{
	m_impl->WriteEndElement();
	return *this;
}

XmlWriter & XmlWriter::WriteAttribute(const QString & name, const QString & value)
{
	m_impl->WriteAttribute(name, value);
	return *this;
}

XmlWriter & XmlWriter::WriteCharacters(const QString & data)
{
	m_impl->WriteCharacters(data);
	return *this;
}
