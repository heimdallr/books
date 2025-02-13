#include "Fb2Parser.h"

#include "fnd/FindPair.h"

#include "util/xml/SaxParser.h"
#include "util/xml/XmlAttributes.h"

#include "log.h"

using namespace HomeCompa;
using namespace Inpx;

namespace
{

constexpr auto NAME = "name";
constexpr auto NUMBER = "number";
constexpr auto DESCRIPTION = "FictionBook/description";
constexpr auto GENRE = "FictionBook/description/title-info/genre";
constexpr auto AUTHOR = "FictionBook/description/title-info/author";
constexpr auto AUTHOR_FIRST_NAME = "FictionBook/description/title-info/author/first-name";
constexpr auto AUTHOR_LAST_NAME = "FictionBook/description/title-info/author/last-name";
constexpr auto AUTHOR_MIDDLE_NAME = "FictionBook/description/title-info/author/middle-name";
constexpr auto BOOK_TITLE = "FictionBook/description/title-info/book-title";
constexpr auto LANG = "FictionBook/description/title-info/lang";
constexpr auto SEQUENCE = "FictionBook/description/title-info/sequence";
constexpr auto KEYWORDS = "FictionBook/description/title-info/keywords";

}

class Fb2Parser::Impl final : public Util::SaxParser
{
public:
	explicit Impl(QIODevice& stream, const QString& fileName)
		: SaxParser(stream, 512)
		, m_fileName(fileName)
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
	bool OnStartElement(const QString& /*name*/, const QString& path, const Util::XmlAttributes& attributes) override
	{
		using ParseElementFunction = bool (Impl::*)(const Util::XmlAttributes&);
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{   AUTHOR,   &Impl::OnStartElementAuthor },
			{ SEQUENCE, &Impl::OnStartElementSequence },
		};

		return Parse(*this, PARSERS, path, attributes);
	}

	bool OnEndElement(const QString& /*name*/, const QString& path) override
	{
		using ParseElementFunction = bool (Impl::*)();
		using ParseElementItem = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{ DESCRIPTION, &Impl::OnEndElementDescription },
		};

		return Parse(*this, PARSERS, path);
	}

	bool OnCharacters(const QString& path, const QString& value) override
	{
		using ParseCharacterFunction = bool (Impl::*)(const QString&);
		using ParseCharacterItem = std::pair<const char*, ParseCharacterFunction>;
		static constexpr ParseCharacterItem PARSERS[] {
			{			  GENRE,            &Impl::ParseGenre },
			{  AUTHOR_FIRST_NAME,  &Impl::ParseAuthorFirstName },
			{   AUTHOR_LAST_NAME,   &Impl::ParseAuthorLastName },
			{ AUTHOR_MIDDLE_NAME, &Impl::ParseAuthorMiddleName },
			{         BOOK_TITLE,        &Impl::ParseBookTitle },
			{			   LANG,             &Impl::ParseLang },
			{           KEYWORDS,         &Impl::ParseKeywords },
		};

		return Parse(*this, PARSERS, path, value);
	}

	bool OnWarning(const QString& text) override
	{
		PLOGW << m_fileName << ": " << text;
		return true;
	}

	bool OnError(const QString& text) override
	{
		m_data.error = text;
		PLOGE << m_fileName << ": " << text;
		return false;
	}

	bool OnFatalError(const QString& text) override
	{
		return OnError(text);
	}

private:
	bool OnStartElementAuthor(const Util::XmlAttributes&)
	{
		m_data.authors.emplace_back();
		return true;
	}

	bool OnStartElementSequence(const Util::XmlAttributes& attributes)
	{
		m_data.series = attributes.GetAttribute(NAME);
		m_data.seqNumber = attributes.GetAttribute(NUMBER).toInt();
		return true;
	}

	bool OnEndElementDescription()
	{
		return false;
	}

	bool ParseGenre(const QString& value)
	{
		m_data.genres.push_back(value);
		return true;
	}

	bool ParseAuthorFirstName(const QString& value)
	{
		m_data.authors.back().first = value;
		return true;
	}

	bool ParseAuthorLastName(const QString& value)
	{
		m_data.authors.back().last = value;
		return true;
	}

	bool ParseAuthorMiddleName(const QString& value)
	{
		m_data.authors.back().middle = value;
		return true;
	}

	bool ParseBookTitle(const QString& value)
	{
		m_data.title = value;
		return true;
	}

	bool ParseLang(const QString& value)
	{
		m_data.lang = value;
		return true;
	}

	bool ParseKeywords(const QString& value)
	{
		m_data.keywords = value;
		return true;
	}

private:
	const QString& m_fileName;
	Data m_data {};
};

Fb2Parser::Fb2Parser(QIODevice& stream, const QString& fileName)
	: m_impl(stream, fileName)
{
}

Fb2Parser::~Fb2Parser() = default;

Fb2Parser::Data Fb2Parser::Parse()
{
	return m_impl->GetData();
}
