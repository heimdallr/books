#include "XmlWriter.h"

#include <xercesc/framework/XMLFormatter.hpp>
#include <xercesc/util/XMLUniDefs.hpp>

#include <QIODevice>

#include "XmlAttributes.h"

using namespace HomeCompa::Util;
using namespace xercesc_3_2;

namespace {
constexpr XMLCh gEndElement[] = { chOpenAngle, chForwardSlash, chNull };
constexpr XMLCh gEndPI[] = { chQuestion, chCloseAngle, chNull };
constexpr XMLCh gStartPI[] = { chOpenAngle, chQuestion, chNull };
constexpr XMLCh gXMLDecl1[] =
{
		chOpenAngle, chQuestion, chLatin_x, chLatin_m, chLatin_l
	,   chSpace, chLatin_v, chLatin_e, chLatin_r, chLatin_s, chLatin_i
	,   chLatin_o, chLatin_n, chEqual, chDoubleQuote, chDigit_1, chPeriod
	,   chDigit_0, chDoubleQuote, chSpace, chLatin_e, chLatin_n, chLatin_c
	,   chLatin_o, chLatin_d, chLatin_i, chLatin_n, chLatin_g, chEqual
	,   chDoubleQuote, chNull
};

constexpr XMLCh gXMLDecl2[] =
{
	chDoubleQuote, chQuestion, chCloseAngle, chLF, chNull
};

}

class XmlWriter::Impl final
	: public XMLFormatTarget
{
public:
	explicit Impl(QIODevice & stream)
		: m_stream(stream)
		, m_formatter("utf-8", this, XMLFormatter::NoEscapes, XMLFormatter::UnRep_CharRef)
	{
		m_formatter << gXMLDecl1 << m_formatter.getEncodingName() << gXMLDecl2;
	}

	void WriteProcessingInstruction(const QString & target, const QString & data)
	{
		m_formatter << XMLFormatter::NoEscapes << gStartPI << target.toStdU16String().data();
		if (!data.isEmpty())
			m_formatter << chSpace << data.toStdU16String().data();

		m_formatter << XMLFormatter::NoEscapes << gEndPI;
	}

	void WriteStartElement(const QString & name, const XmlAttributes & attributes)
	{
		m_formatter << XMLFormatter::NoEscapes << chOpenAngle << name.toStdU16String().data();

		for (size_t i = 0, sz = attributes.GetCount(); i < sz; ++i)
			m_formatter << XMLFormatter::NoEscapes
			<< chSpace << attributes.GetName(i).toStdU16String().data()
			<< chEqual << chDoubleQuote
			<< XMLFormatter::AttrEscapes
			<< attributes.GetValue(i).toStdU16String().data()
			<< XMLFormatter::NoEscapes
			<< chDoubleQuote;

		m_formatter << chCloseAngle;
	}

	void WriteEndElement(const QString & name)
	{
		m_formatter << XMLFormatter::NoEscapes << gEndElement << name.toStdU16String().data() << chCloseAngle;
	}

	void WriteCharacters(const QString & data)
	{
		const auto chars = data.toStdU16String();
		m_formatter.formatBuf(chars.data(), chars.length(), XMLFormatter::CharEscapes);
	}

private: // XMLFormatTarget
	void writeChars(const XMLByte * const toWrite, const XMLSize_t count, XMLFormatter * const) override
	{
		m_stream.write(reinterpret_cast<const char *>(toWrite), static_cast<qint64>(count));
	}

private:
	QIODevice & m_stream;
	XMLFormatter m_formatter;
};

XmlWriter::XmlWriter(QIODevice & stream)
	: m_impl(stream)
{
}

XmlWriter::~XmlWriter() = default;

void XmlWriter::WriteProcessingInstruction(const QString & target, const QString & data)
{
	m_impl->WriteProcessingInstruction(target, data);
}

void XmlWriter::WriteStartElement(const QString & name, const XmlAttributes & attributes)
{
	m_impl->WriteStartElement(name, attributes);
}

void XmlWriter::WriteEndElement(const QString & name)
{
	m_impl->WriteEndElement(name);
}

void XmlWriter::WriteCharacters(const QString & data)
{
	m_impl->WriteCharacters(data);
}
