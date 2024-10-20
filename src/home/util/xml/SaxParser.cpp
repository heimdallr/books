#include "SaxParser.h"

#include <xercesc/parsers/SAXParser.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/sax/InputSource.hpp>
#include <xercesc/util/BinInputStream.hpp>

#include <QIODevice>
#include <QStringList>

#include "fnd/ScopedCall.h"

#include "Initializer.h"
#include "XmlAttributes.h"

using namespace HomeCompa;
using namespace Util;
namespace xercesc = xercesc_3_2;

namespace {

class XmlAttributesImpl final : public XmlAttributes
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

	size_t GetCount() const override
	{
		return m_attributes->getLength();
	}

	QString GetName(const size_t index) const override
	{
		assert(index < GetCount());
		return QString::fromStdU16String(m_attributes->getName(index));
	}

	QString GetValue(const size_t index) const override
	{
		assert(index < GetCount());
		return QString::fromStdU16String(m_attributes->getValue(index));
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
	BinInputStream(QIODevice & source, const int64_t maxChunkSize)
		: m_source(source)
		, m_maxChunkSize(maxChunkSize)
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
		return m_stopped ? 0 : m_source.read(reinterpret_cast<char *>(toFill), std::min(static_cast<int64_t>(maxToRead), m_maxChunkSize));
	}

private:
	QIODevice & m_source;
	const int64_t m_maxChunkSize;
	bool m_stopped { false };
};

class InputSource final : public xercesc::InputSource
{
public:
	InputSource(QIODevice & source, const int64_t maxChunkSize)
		: m_binInputStream(new BinInputStream(source, maxChunkSize))
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

class SaxHandler final
	: public xercesc::HandlerBase
{
public:
	explicit SaxHandler(SaxParser & parser, InputSource & inputSource)
		: m_parser(parser)
		, m_inputSource(inputSource)
	{
	}
private: // xercesc::DocumentHandler
	void processingInstruction(const  XMLCh * const target, const XMLCh * const data) override
	{
		if (m_inputSource.IsStopped())
			return;

		if (m_parser.OnProcessingInstruction(QString::fromStdU16String(target), QString::fromStdU16String(data)))
			m_inputSource.SetStopped(true);
	}

	void startElement(const XMLCh * const name, xercesc::AttributeList & args) override
	{
		ProcessCharacters();
		if (m_inputSource.IsStopped())
			return;

		m_stack.Push(name);
		const auto & key = m_stack.ToString();
		m_attributes.SetAttributeList(args);
		if (!m_parser.OnStartElement(QString::fromStdU16String(name), key, m_attributes))
			m_inputSource.SetStopped(true);
	}

	void endElement(const XMLCh * const name) override
	{
		if (m_inputSource.IsStopped())
			return;

		ProcessCharacters();

		if (const auto & key = m_stack.ToString(); !m_parser.OnEndElement(QString::fromStdU16String(name), key))
			m_inputSource.SetStopped(true);

		m_stack.Pop(name);
	}

	void characters(const XMLCh * const chars, const XMLSize_t length) override
	{
		if (m_inputSource.IsStopped())
			return;

		if (length != 0)
			m_characters.append(QString::fromStdU16String(chars));
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
	void ProcessCharacters()
	{
		if (m_inputSource.IsStopped())
			return;

		ScopedCall clearGuard([&] { m_characters.clear(); });

		if (m_characters.simplified().isEmpty())
			return;

		if (const auto & key = m_stack.ToString(); !m_parser.OnCharacters(key, m_characters))
			m_inputSource.SetStopped(true);
	}

private:
	XmlStack m_stack;
	XmlAttributesImpl m_attributes{};

	SaxParser & m_parser;
	InputSource & m_inputSource;
	QString m_characters;
};

}

class SaxParser::Impl
{
public:
	explicit Impl(SaxParser & self, QIODevice & stream, const int64_t maxChunkSize)
		: m_self(self)
		, m_inputSource(stream, maxChunkSize)
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
	XMLPlatformInitializer m_initializer;
	xercesc::SAXParser m_saxParser;
	SaxParser & m_self;
	InputSource m_inputSource;
};

SaxParser::SaxParser(QIODevice & stream, const int64_t maxChunkSize)
	: m_impl(*this, stream, maxChunkSize)
{
}

SaxParser::~SaxParser() = default;

void SaxParser::Parse()
{
	m_impl->Parse();
}

bool SaxParser::IsLastItemProcessed() const noexcept
{
	return m_processed;
}