#include "Fb2Parser.h"

#include <QDateTime>
#include <QIODevice>

#include <xercesc/parsers/SAXParser.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/sax/InputSource.hpp>
#include <xercesc/util/BinInputStream.hpp>
#include <xercesc/util/PlatformUtils.hpp>

#include <plog/Log.h>

#include "fnd/FindPair.h"

using namespace HomeCompa;
using namespace Inpx;

namespace {

constexpr auto NAME = "name";
constexpr auto NUMBER = "number";
constexpr auto VALUE = "value";
constexpr auto DESCRIPTION = "FictionBook/description";
constexpr auto GENRE = "FictionBook/description/title-info/genre";
constexpr auto AUTHOR = "FictionBook/description/title-info/author";
constexpr auto AUTHOR_FIRST_NAME = "FictionBook/description/title-info/author/first-name";
constexpr auto AUTHOR_LAST_NAME = "FictionBook/description/title-info/author/last-name";
constexpr auto AUTHOR_MIDDLE_NAME = "FictionBook/description/title-info/author/middle-name";
constexpr auto BOOK_TITLE = "FictionBook/description/title-info/book-title";
constexpr auto DATE = "FictionBook/description/title-info/date";
constexpr auto LANG = "FictionBook/description/title-info/lang";
constexpr auto SEQUENCE = "FictionBook/description/title-info/sequence";
constexpr auto DOCUMENT_INFO_DATE = "FictionBook/description/document-info/date";

struct PszComparerEndsWithCaseInsensitive
{
	bool operator()(const std::string_view lhs, const std::string_view rhs) const
	{
		if (lhs.size() < rhs.size())
			return false;

		const auto * lp = lhs.data() + (lhs.size() - rhs.size()), * rp = rhs.data();
		while (*lp && *rp && std::tolower(*lp++) == std::tolower(*rp++))
			;

		return !*lp && !*rp;
	}
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

namespace xercesc = xercesc_3_2;

class BinInputStream final : public xercesc::BinInputStream
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

private: // xercesc::BinInputStream
	XMLFilePos curPos() const override
	{
		return m_source.pos();
	}

	const XMLCh* getContentType() const override
	{
		return nullptr;
	}

	XMLSize_t readBytes(XMLByte* const toFill, const XMLSize_t maxToRead) override
	{
		return m_stopped ? 0 : m_source.read(reinterpret_cast<char*>(toFill), static_cast<qint64>(maxToRead));
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

private: // xercesc::InputSource
	xercesc::BinInputStream* makeStream() const override
	{
		return m_binInputStream;
	}

private:
	BinInputStream * m_binInputStream;
};

class SaxHandler : public xercesc::HandlerBase
{
public:
	explicit SaxHandler(const QString & fileName, InputSource & inputSource)
		: m_fileName(fileName)
		, m_inputSource(inputSource)
	{
	}

	Fb2Parser::Data GetData()
	{
		auto data = std::move(m_data);
		m_data = {};
		return data;
	}

private: // xercesc::DocumentHandler
	void startElement(const XMLCh* const name, xercesc::AttributeList& args) override
	{
		m_stack.Push(name);

		using ParseElementFunction = void(SaxHandler::*)(xercesc::AttributeList & attrs);
		using ParseElementItem = std::pair<const char *, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[]
		{
			{ AUTHOR            , &SaxHandler::OnStartElementAuthor },
			{ SEQUENCE          , &SaxHandler::OnStartElementSequence },
			{ DOCUMENT_INFO_DATE, &SaxHandler::OnStartDocumentInfoDate },
		};

		Parse(PARSERS, args);
	}

	void endElement(const XMLCh* const name) override
	{
		using ParseElementFunction = void(SaxHandler::*)();
		using ParseElementItem = std::pair<const char *, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[]
		{
			{ DESCRIPTION, &SaxHandler::OnEndElementDescription },
		};

		Parse(PARSERS);

		m_stack.Pop(name);
	}

	void characters(const XMLCh* const chars, const XMLSize_t length) override
	{
		if (length == 0)
			return;

		auto textValue = QString::fromStdU16String(chars).simplified();
		if (textValue.isEmpty())
			return;

		using ParseCharacterFunction = void(SaxHandler::*)(QString &&);
		using ParseCharacterItem = std::pair<const char *, ParseCharacterFunction>;
		static constexpr ParseCharacterItem PARSERS[]
		{
			{ GENRE             , &SaxHandler::ParseGenre },
			{ AUTHOR_FIRST_NAME , &SaxHandler::ParseAuthorFirstName },
			{ AUTHOR_LAST_NAME  , &SaxHandler::ParseAuthorLastName },
			{ AUTHOR_MIDDLE_NAME, &SaxHandler::ParseAuthorMiddleName },
			{ BOOK_TITLE        , &SaxHandler::ParseBookTitle },
			{ DATE              , &SaxHandler::ParseDate },
			{ LANG              , &SaxHandler::ParseLang },
			{ DOCUMENT_INFO_DATE, &SaxHandler::ParseDocumentInfoDate },
		};

		Parse(PARSERS, std::move(textValue));
	}

private: // xercesc::ErrorHandler
	void warning(const xercesc::SAXParseException & exc) override
	{
		if (m_stopped)
			return;

		PLOGW << m_fileName << ": " << QString::fromStdU16String(exc.getMessage());
	}

	void error(const xercesc::SAXParseException & exc) override
	{
		if (m_stopped)
			return;

		m_data.error = QString::fromStdU16String(exc.getMessage());
		PLOGE << m_fileName << ": " << m_data.error;
	}

	void fatalError(const xercesc::SAXParseException & exc) override
	{
		error(exc);
	}

private:
	template<typename... ARGS>
	// ReSharper disable once CppMemberFunctionMayBeStatic
	void Stub(ARGS &&...)
	{
	}

	void OnStartElementAuthor(xercesc::AttributeList &)
	{
		m_data.authors.emplace_back();
	}

	void OnStartElementSequence(xercesc::AttributeList & attrs)
	{
		if (auto * value = attrs.getValue(NAME))
			m_data.series = QString::fromStdU16String(value);
		if (auto * value = attrs.getValue(NUMBER))
			m_data.seqNumber = QString::fromStdU16String(value).toInt();
	}

	void OnStartDocumentInfoDate(xercesc::AttributeList & attrs)
	{
		if (auto * value = attrs.getValue(VALUE))
			m_data.date = QString::fromStdU16String(value);
	}

	void OnEndElementDescription()
	{
		m_stopped = true;
		m_inputSource.SetStopped(true);
	}

	void ParseGenre(QString && value)
	{
		m_data.genres.push_back(std::move(value));
	}

	void ParseAuthorFirstName(QString && value)
	{
		m_data.authors.back().first = std::move(value);
	}

	void ParseAuthorLastName(QString && value)
	{
		m_data.authors.back().last = std::move(value);
	}

	void ParseAuthorMiddleName(QString && value)
	{
		m_data.authors.back().middle = std::move(value);
	}

	void ParseBookTitle(QString && value)
	{
		m_data.title = std::move(value);
	}

	void ParseDate(QString && value)
	{
		m_data.date = std::move(value);
		m_data.date.replace(".", "-");
	}

	void ParseDocumentInfoDate(QString && value)
	{
		if (!m_data.date.isEmpty())
			return;

		m_data.date = std::move(value);
	}

	void ParseLang(QString && value)
	{
		m_data.lang = std::move(value);
	}

	template <typename Value, size_t ArraySize, typename... ARGS>
	void Parse(Value(&array)[ArraySize], ARGS &&... args)
	{
		const auto & key = m_stack.ToString();
		[[maybe_unused]]const auto parser = FindSecond(array, key.toStdString().data(), &SaxHandler::Stub<ARGS...>, PszComparerEndsWithCaseInsensitive {});
		std::invoke(parser, *this, std::forward<ARGS>(args)...);
	}

private:
	XmlStack m_stack;
	const QString & m_fileName;
	InputSource & m_inputSource;
	bool m_stopped { false };

	Fb2Parser::Data m_data {};
};

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

class Parser
{
public:
	explicit Parser(QIODevice & stream)
		: m_inputSource(stream)
	{
		m_saxParser.setValidationScheme(xercesc::SAXParser::Val_Auto);
		m_saxParser.setDoNamespaces(false);
		m_saxParser.setDoSchema(false);
		m_saxParser.setHandleMultipleImports(true);
		m_saxParser.setValidationSchemaFullChecking(false);
	}

	Fb2Parser::Data Parse(const QString & fileName)
	{
		SaxHandler handler(fileName, m_inputSource);
		m_saxParser.setDocumentHandler(&handler);
		m_saxParser.setErrorHandler(&handler);

		m_saxParser.parse(m_inputSource);

		return handler.GetData();
	}

private:
	xercesc::SAXParser m_saxParser;
	InputSource m_inputSource;
};


}

struct Fb2Parser::Impl
{
private:
	QIODevice & m_stream;

public:
	Parser parser;
	explicit Impl(QIODevice & stream)
		: m_stream(stream)
		, parser(m_stream)
	{
	}
};

Fb2Parser::Fb2Parser(QIODevice & stream)
	: m_impl(stream)
{
}

Fb2Parser::~Fb2Parser() = default;

Fb2Parser::Data Fb2Parser::Parse(const QString & fileName)
{
	return m_impl->parser.Parse(fileName);
}
