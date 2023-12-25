#include "SaxParser.h"

#include <xercesc/parsers/SAXParser.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/sax/InputSource.hpp>
#include <xercesc/util/BinInputStream.hpp>
#include <xercesc/util/PlatformUtils.hpp>

#include <QIODevice>
#include <QStringList>

using namespace HomeCompa::Util;
namespace xercesc = xercesc_3_2;

namespace {

class XMLPlatformInitializer
{
	NON_COPY_MOVABLE(XMLPlatformInitializer)

public:
	XMLPlatformInitializer()
	{
		xercesc::XMLPlatformUtils::Initialize();
	}

	~XMLPlatformInitializer()
	{
		xercesc::XMLPlatformUtils::Terminate();
	}
};

[[maybe_unused]] XMLPlatformInitializer INITIALIZER;

class AttributesImpl final : public SaxParser::Attributes
{
public:
	void SetAttributeList(const xercesc::AttributeList & attributes) noexcept
	{
		m_attributes = &attributes;
	}

private: // SaxParser::Attributes
	QString GetAttribute(const QString & key) const override
	{
		if (const auto value = m_attributes->getValue(key.toStdU16String().data()))
			return QString::fromStdU16String(value);
		return {};
	}

private:
	const xercesc::AttributeList * m_attributes { nullptr };
};

class XmlStack
{
public:
	void Push(const QStringView tag)
	{
		m_data.push_back(tag.toString());
		m_key.reset();
	}

	void Pop(const QStringView tag)
	{
		assert(!m_data.isEmpty());
		if (tag != m_data.back())
			return;

		m_data.pop_back();
		m_key.reset();
	}

	const QString & ToString() const
	{
		if (!m_key)
			m_key = m_data.join('/');

		return *m_key;
	}

private:
	mutable std::optional<QString> m_key;
	QStringList m_data;
};

class BinInputStream final : public xercesc_3_2::BinInputStream
{
public:
	explicit BinInputStream(QIODevice & source)
		: m_source(source)
	{
	}

	void SetStopped(const bool value) noexcept
	{
		m_stopped = value;
	}

	bool IsStopped() const noexcept
	{
		return m_stopped;
	}

private: // xercesc::BinInputStream
	XMLFilePos curPos() const override
	{
		return m_source.pos();
	}

	const XMLCh * getContentType() const override
	{
		return nullptr;
	}

	XMLSize_t readBytes(XMLByte * const toFill, const XMLSize_t maxToRead) override
	{
		return m_stopped ? 0 : m_source.read(reinterpret_cast<char *>(toFill), static_cast<qint64>(maxToRead));
	}

private:
	QIODevice & m_source;
	bool m_stopped { false };
};

class InputSource final : public xercesc::InputSource
{
public:
	explicit InputSource(QIODevice & source)
		: m_binInputStream(new BinInputStream(source))
	{
	}

	void SetStopped(const bool value) const noexcept
	{
		m_binInputStream->SetStopped(value);
	}

	bool IsStopped() const noexcept
	{
		return m_binInputStream->IsStopped();
	}

private: // xercesc::InputSource
	xercesc::BinInputStream * makeStream() const override
	{
		return m_binInputStream;
	}

private:
	BinInputStream * m_binInputStream;
};

class SaxHandler final : public xercesc::HandlerBase
{
public:
	explicit SaxHandler(SaxParser & parser, InputSource & inputSource)
		: m_parser(parser)
		, m_inputSource(inputSource)
	{
	}
private: // xercesc::DocumentHandler
	void startElement(const XMLCh * const name, xercesc::AttributeList & args) override
	{
		m_stack.Push(name);
		const auto & key = m_stack.ToString();
		m_attributes.SetAttributeList(args);
		if (!m_parser.OnStartElement(key, m_attributes))
			m_inputSource.SetStopped(true);
	}

	void endElement(const XMLCh * const name) override
	{
		if (const auto & key = m_stack.ToString(); !m_parser.OnEndElement(key))
			m_inputSource.SetStopped(true);

		m_stack.Pop(name);
	}

	void characters(const XMLCh * const chars, const XMLSize_t length) override
	{
		if (length == 0)
			return;

		const auto textValue = QString::fromStdU16String(chars).simplified();
		if (textValue.isEmpty())
			return;

		if (const auto & key = m_stack.ToString(); !m_parser.OnCharacters(key, textValue))
			m_inputSource.SetStopped(true);
	}

private: // xercesc::ErrorHandler
	void warning(const xercesc::SAXParseException & exc) override
	{
		if (m_inputSource.IsStopped())
			return;

		if (!m_parser.OnWarning(QString::fromStdU16String(exc.getMessage())))
			m_inputSource.SetStopped(true);
	}

	void error(const xercesc::SAXParseException & exc) override
	{
		if (m_inputSource.IsStopped())
			return;

		if (!m_parser.OnError(QString::fromStdU16String(exc.getMessage())))
			m_inputSource.SetStopped(true);
	}

	void fatalError(const xercesc::SAXParseException & exc) override
	{
		if (m_inputSource.IsStopped())
			return;

		if (!m_parser.OnFatalError(QString::fromStdU16String(exc.getMessage())))
			m_inputSource.SetStopped(true);
	}

private:
	XmlStack m_stack;
	AttributesImpl m_attributes{};

	SaxParser & m_parser;
	InputSource & m_inputSource;
};

}

class SaxParser::Impl
{
public:
	explicit Impl(SaxParser & self, QIODevice & stream)
		: m_self(self)
		, m_inputSource(stream)
	{
		m_saxParser.setValidationScheme(xercesc::SAXParser::Val_Auto);
		m_saxParser.setDoNamespaces(false);
		m_saxParser.setDoSchema(false);
		m_saxParser.setHandleMultipleImports(true);
		m_saxParser.setValidationSchemaFullChecking(false);
	}

	void Parse()
	{
		SaxHandler handler(m_self, m_inputSource);
		m_saxParser.setDocumentHandler(&handler);
		m_saxParser.setErrorHandler(&handler);

		m_saxParser.parse(m_inputSource);
	}

private:
	SaxParser & m_self;
	xercesc::SAXParser m_saxParser;
	InputSource m_inputSource;
};

SaxParser::SaxParser(QIODevice & stream)
	: m_impl(*this, stream)
{
}

SaxParser::~SaxParser() = default;

void SaxParser::Parse()
{
	m_impl->Parse();
}
