#include "Fb2Parser.h"

#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "util/xml/SaxParser.h"
#include "util/xml/XmlAttributes.h"

using namespace HomeCompa;
using namespace fb2imager;

namespace {

constexpr auto ID = "id";
constexpr auto GENRE = "FictionBook/description/title-info/genre";
constexpr auto BOOK_TITLE = "FictionBook/description/title-info/book-title";
constexpr auto LANG = "FictionBook/description/title-info/lang";
constexpr auto BINARY = "FictionBook/binary";


}

class Fb2Parser::Impl final
	: public Util::SaxParser
{
public:
	explicit Impl(QIODevice & stream, QString fileName, OnBinaryFound binaryCallback)
		: SaxParser(stream, 512)
		, m_fileName(std::move(fileName))
		, m_binaryCallback(std::move(binaryCallback))
	{
	}

	Data GetData()
	{
		Parse();

		auto data = std::move(m_data);
		m_data = {};
		return data;
	}

private: // Util::SaxParser
	bool OnStartElement(const QString & /*name*/, const QString & path, const Util::XmlAttributes & attributes) override
	{
		using ParseElementFunction = bool(Impl::*)(const Util::XmlAttributes &);
		using ParseElementItem = std::pair<const char *, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[]
		{
			{ BINARY, &Impl::OnStartElementBinary },
		};

		return Parse(*this, PARSERS, path, attributes);
	}

	bool OnEndElement(const QString & /*name*/, const QString & /*path*/) override
	{
		return true;
	}

	bool OnCharacters(const QString & path, const QString & value) override
	{
		using ParseCharacterFunction = bool(Impl::*)(const QString &);
		using ParseCharacterItem = std::pair<const char *, ParseCharacterFunction>;
		static constexpr ParseCharacterItem PARSERS[]
		{
			{ GENRE             , &Impl::ParseGenre },
			{ BOOK_TITLE        , &Impl::ParseBookTitle },
			{ LANG              , &Impl::ParseLang },
			{ BINARY            , &Impl::ParseBinary },
		};

		return Parse(*this, PARSERS, path, value);
	}

	bool OnWarning(const QString & text) override
	{
		PLOGW << m_fileName << ": " << text;
		return true;
	}

	bool OnError(const QString & text) override
	{
		m_data.error = text;
		PLOGE << m_fileName << ": " << text;
		return false;
	}

	bool OnFatalError(const QString & text) override
	{
		return OnError(text);
	}

private:
	bool OnStartElementBinary(const Util::XmlAttributes & attributes)
	{
		m_binaryId = attributes.GetAttribute(ID);
		return true;
	}

	bool ParseGenre(const QString & value)
	{
		m_data.genres.push_back(value);
		return true;
	}

	bool ParseBookTitle(const QString & value)
	{
		m_data.title = value;
		return true;
	}

	bool ParseLang(const QString & value)
	{
		m_data.lang = value;
		return true;
	}

	bool ParseBinary(const QString & value)
	{
		return m_binaryCallback(m_binaryId, QByteArray::fromBase64(value.toUtf8()));
	}

private:
	const QString m_fileName;
	const OnBinaryFound m_binaryCallback;
	QString m_binaryId;
	Data m_data {};
};

Fb2Parser::Fb2Parser(QIODevice & stream, QString fileName, OnBinaryFound binaryCallback)
	: m_impl(stream, std::move(fileName), std::move(binaryCallback))
{
}

Fb2Parser::~Fb2Parser() = default;

Fb2Parser::Data Fb2Parser::Parse()
{
	return m_impl->GetData();
}
