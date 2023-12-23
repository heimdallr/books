#include "Fb2Parser.h"

#include <QDateTime>
#include <QFile>
#include <QRegularExpression>
#include <QXmlStreamReader>

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

}

class Fb2Parser::Impl final
	: QXmlStreamReader
{
	using ParseElementFunction = void(Impl::*)();
	using ParseElementItem = std::pair<const char *, ParseElementFunction>;

public:
	explicit Impl(QIODevice & ioDevice)
		: QXmlStreamReader(&ioDevice)
	{
	}

	Data Parse()
	{
		static constexpr std::pair<TokenType, ParseElementFunction> PARSERS[]
		{
			{ StartElement, &Impl::OnStartElement },
			{ EndElement  , &Impl::OnEndElement },
			{ Characters  , &Impl::OnCharacters },
		};

		while (!m_stop && !atEnd() && !hasError())
		{
			const auto token = readNext();
			if (token == Invalid)
				PLOGE << error() << ":" << errorString();

			const auto parser = FindSecond(PARSERS, token, &Impl::Stub<>);
			std::invoke(parser, this);
		}

		Data data = std::move(m_data);
		m_data = {};
		return data;
	}

private:
	template<typename... ARGS>
	// ReSharper disable once CppMemberFunctionMayBeStatic
	void Stub(ARGS &&...)
	{
	}

	void OnStartElement()
	{
		const auto tag = name();
		m_stack.Push(tag);

		static constexpr ParseElementItem PARSERS[]
		{
			{ AUTHOR            , &Impl::OnStartElementAuthor },
			{ SEQUENCE          , &Impl::OnStartElementSequence },
			{ DOCUMENT_INFO_DATE, &Impl::OnStartDocumentInfoDate },
		};

		Parse(PARSERS);
	}

	void OnEndElement()
	{
		static constexpr ParseElementItem PARSERS[]
		{
			{ DESCRIPTION, &Impl::OnEndElementDescription },
		};

		Parse(PARSERS);

		m_stack.Pop(name());
	}

	void OnCharacters()
	{
		auto textValue = text().toString().simplified();
		if (textValue.isEmpty())
			return;

		using ParseCharacterFunction = void(Impl::*)(QString&&);
		using ParseCharacterItem = std::pair<const char *, ParseCharacterFunction>;
		static constexpr ParseCharacterItem PARSERS[]
		{
			{ GENRE             , &Impl::ParseGenre },
			{ AUTHOR_FIRST_NAME , &Impl::ParseAuthorFirstName },
			{ AUTHOR_LAST_NAME  , &Impl::ParseAuthorLastName },
			{ AUTHOR_MIDDLE_NAME, &Impl::ParseAuthorMiddleName },
			{ BOOK_TITLE        , &Impl::ParseBookTitle },
			{ DATE              , &Impl::ParseDate },
			{ LANG              , &Impl::ParseLang },
			{ DOCUMENT_INFO_DATE, &Impl::ParseDocumentInfoDate },
		};

		Parse(PARSERS, std::move(textValue));
	}

	void OnStartElementAuthor()
	{
		m_data.authors.emplace_back();
	}

	void OnStartElementSequence()
	{
		m_data.series = attributes().value(NAME).toString();
		m_data.seqNumber = attributes().value(NUMBER).toInt();
	}

	void OnStartDocumentInfoDate()
	{
		m_data.date = attributes().value(VALUE).toString();
	}

	void OnEndElementDescription()
	{
		m_stop = true;
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

		constexpr const char * formats[] { "dd.MM.yyyy", "yyyy" };
		for (const auto * format : formats)
		{
			if (const auto date = QDateTime::fromString(m_data.date, format); date.isValid())
			{
				m_data.date = date.toString("yyyy-MM-dd");
				break;
			}
		}
	}

	void ParseLang(QString && value)
	{
		m_data.lang = std::move(value);
	}

	template <typename Value, size_t ArraySize, typename... ARGS>
	void Parse(Value(&array)[ArraySize], ARGS &&... args)
	{
		const auto & key = m_stack.ToString();
		const auto parser = FindSecond(array, key.toStdString().data(), &Impl::Stub<ARGS...>, PszComparerEndsWithCaseInsensitive {});
		std::invoke(parser, *this, std::forward<ARGS>(args)...);
	}

private:
	XmlStack m_stack;
	bool m_stop { false };

	Data m_data{};
};

Fb2Parser::Fb2Parser(QIODevice & stream)
	: m_impl(stream)
{
}

Fb2Parser::~Fb2Parser() = default;

Fb2Parser::Data Fb2Parser::Parse()
{
	return m_impl->Parse();
}
