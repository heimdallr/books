#include "Fb2InpxParser.h"

#include <QFileInfo>

#include "fnd/FindPair.h"

#include "util/xml/SaxParser.h"
#include "util/xml/XmlAttributes.h"

#include "log.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Util;

namespace
{

constexpr auto NAME                   = "name";
constexpr auto NUMBER                 = "number";
constexpr auto DESCRIPTION            = "FictionBook/description";
constexpr auto GENRE                  = "FictionBook/description/title-info/genre";
constexpr auto AUTHOR                 = "FictionBook/description/title-info/author";
constexpr auto AUTHOR_FIRST_NAME      = "FictionBook/description/title-info/author/first-name";
constexpr auto AUTHOR_LAST_NAME       = "FictionBook/description/title-info/author/last-name";
constexpr auto AUTHOR_MIDDLE_NAME     = "FictionBook/description/title-info/author/middle-name";
constexpr auto AUTHOR_DOC             = "FictionBook/description/document-info/author";
constexpr auto AUTHOR_FIRST_NAME_DOC  = "FictionBook/description/document-info/author/first-name";
constexpr auto AUTHOR_LAST_NAME_DOC   = "FictionBook/description/document-info/author/last-name";
constexpr auto AUTHOR_MIDDLE_NAME_DOC = "FictionBook/description/document-info/author/middle-name";
constexpr auto BOOK_TITLE             = "FictionBook/description/title-info/book-title";
constexpr auto LANG                   = "FictionBook/description/title-info/lang";
constexpr auto SEQUENCE               = "FictionBook/description/title-info/sequence";
constexpr auto KEYWORDS               = "FictionBook/description/title-info/keywords";
constexpr auto PUBLISH_INFO_YEAR      = "FictionBook/description/publish-info/year";

QString AuthorsToString(const Fb2InpxParser::Data::Authors& authors)
{
	QStringList values;
	values.reserve(static_cast<int>(authors.size()));
	std::ranges::transform(authors, std::back_inserter(values), [](const auto& author) {
		return (QStringList() << author.last << author.first << author.middle).join(Fb2InpxParser::NAMES_SEPARATOR);
	});
	return values.join(Fb2InpxParser::LIST_SEPARATOR) + Fb2InpxParser::LIST_SEPARATOR;
}

QString GenresToString(const QStringList& genres)
{
	return genres.empty() ? QString {} : genres.join(Fb2InpxParser::LIST_SEPARATOR) + Fb2InpxParser::LIST_SEPARATOR;
}

} // namespace

class Fb2InpxParser::Impl final : public SaxParser
{
public:
	Impl(QIODevice& stream, const QString& fileName)
		: SaxParser(stream, 512)
		, m_fileName(fileName)
	{
	}

	Data GetData()
	{
		Parse();

		auto data = std::move(m_data);
		m_data    = {};
		return data;
	}

private: // SaxParser
	bool OnStartElement(const QString& /*name*/, const QString& path, const XmlAttributes& attributes) override
	{
		using ParseElementFunction = bool (Impl::*)(const XmlAttributes&);
		using ParseElementItem     = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{     AUTHOR,    &Impl::OnStartElementAuthor },
			{ AUTHOR_DOC, &Impl::OnStartElementAuthorDoc },
			{   SEQUENCE,  &Impl::OnStartElementSequence },
		};

		return Parse(*this, PARSERS, path, attributes);
	}

	bool OnEndElement(const QString& /*name*/, const QString& path) override
	{
		using ParseElementFunction = bool (Impl::*)();
		using ParseElementItem     = std::pair<const char*, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[] {
			{ DESCRIPTION, &Impl::OnEndElementDescription },
			{      AUTHOR,      &Impl::OnEndElementAuthor },
			{  AUTHOR_DOC,      &Impl::OnEndElementAuthor },
		};

		return Parse(*this, PARSERS, path);
	}

	bool OnCharacters(const QString& path, const QString& value) override
	{
		using ParseCharacterFunction = bool (Impl::*)(const QString&);
		using ParseCharacterItem     = std::pair<const char*, ParseCharacterFunction>;
		static constexpr ParseCharacterItem PARSERS[] {
			{				  GENRE,            &Impl::ParseGenre },
			{      AUTHOR_FIRST_NAME,  &Impl::ParseAuthorFirstName },
			{       AUTHOR_LAST_NAME,   &Impl::ParseAuthorLastName },
			{     AUTHOR_MIDDLE_NAME, &Impl::ParseAuthorMiddleName },
			{  AUTHOR_FIRST_NAME_DOC,  &Impl::ParseAuthorFirstName },
			{   AUTHOR_LAST_NAME_DOC,   &Impl::ParseAuthorLastName },
			{ AUTHOR_MIDDLE_NAME_DOC, &Impl::ParseAuthorMiddleName },
			{             BOOK_TITLE,        &Impl::ParseBookTitle },
			{				   LANG,             &Impl::ParseLang },
			{			   KEYWORDS,         &Impl::ParseKeywords },
			{      PUBLISH_INFO_YEAR,      &Impl::ParsePublishYear },
		};

		return Parse(*this, PARSERS, path, value);
	}

	bool OnWarning(const size_t line, const size_t column, const QString& text) override
	{
		PLOGW << m_fileName << " " << line << ":" << column << " " << text;
		return true;
	}

	bool OnError(const size_t line, const size_t column, const QString& text) override
	{
		m_data.error = text;
		PLOGE << m_fileName << " " << line << ":" << column << " " << text;
		return false;
	}

	bool OnFatalError(const size_t line, const size_t column, const QString& text) override
	{
		return OnError(line, column, text);
	}

private:
	bool OnStartElementAuthor(const XmlAttributes&)
	{
		m_insertAuthorMode = true;
		m_data.authors.emplace_back();
		return true;
	}

	bool OnStartElementAuthorDoc(const XmlAttributes& attributes)
	{
		return m_data.authors.empty() ? OnStartElementAuthor(attributes) : true;
	}

	bool OnStartElementSequence(const XmlAttributes& attributes)
	{
		m_data.series    = attributes.GetAttribute(NAME);
		m_data.seqNumber = GetSeqNumber(attributes.GetAttribute(NUMBER));
		return true;
	}

	bool OnEndElementDescription()
	{
		return false;
	}

	bool OnEndElementAuthor()
	{
		if (m_insertAuthorMode)
		{
			assert(!m_data.authors.empty());
			const auto& author = m_data.authors.back();
			if (author.first.isEmpty() && author.last.isEmpty() && author.middle.isEmpty())
				m_data.authors.pop_back();
		}

		m_insertAuthorMode = false;
		return true;
	}

	bool ParseGenre(const QString& value)
	{
		m_data.genres.push_back(value);
		return true;
	}

	bool ParseAuthorFirstName(const QString& value)
	{
		if (m_insertAuthorMode)
			m_data.authors.back().first = value;
		return true;
	}

	bool ParseAuthorLastName(const QString& value)
	{
		if (m_insertAuthorMode)
			m_data.authors.back().last = value;
		return true;
	}

	bool ParseAuthorMiddleName(const QString& value)
	{
		if (m_insertAuthorMode)
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

	bool ParsePublishYear(const QString& value)
	{
		m_data.year = value;
		return true;
	}

private:
	const QString& m_fileName;
	Data           m_data {};
	bool           m_insertAuthorMode { false };
};

Fb2InpxParser::Fb2InpxParser(QIODevice& stream, const QString& fileName)
	: m_impl(stream, fileName)
{
}

Fb2InpxParser::~Fb2InpxParser() = default;

QString Fb2InpxParser::Parse(const QString& folder, const Zip& zip, const QString& fileName, const QDateTime& zipDateTime, const bool isDeleted)
{
	try
	{
		QFileInfo     fileInfo(fileName);
		const auto    stream = zip.Read(fileName);
		Fb2InpxParser parser(stream->GetStream(), fileName);
		const auto    parserData = parser.m_impl->GetData();

		if (!parserData.error.isEmpty())
		{
			PLOGE << QString("%1/%2: %3").arg(folder, fileName, parserData.error);
			return {};
		}

		if (parserData.authors.empty())
			PLOGW << QString("%1/%2: author empty").arg(folder, fileName);

		if (parserData.title.isEmpty())
			PLOGW << QString("%1/%2: title empty").arg(folder, fileName);

		if (zip.GetFileSize(fileName) == 0)
			PLOGW << QString("%1/%2: file empty").arg(folder, fileName);

		const auto& fileDateTime = zip.GetFileTime(fileName);
		auto        dateTime     = (fileDateTime.isValid() ? fileDateTime : zipDateTime).toString("yyyy-MM-dd");

		const auto values = QStringList() << AuthorsToString(parserData.authors) << GenresToString(parserData.genres) << parserData.title << parserData.series << parserData.seqNumber
		                                  << fileInfo.completeBaseName() << QString::number(zip.GetFileSize(fileName)) << fileInfo.completeBaseName() << (isDeleted ? "1" : "0") << fileInfo.suffix()
		                                  << std::move(dateTime) << parserData.lang << "0" << parserData.keywords << parserData.year;

		return values.join(FIELDS_SEPARATOR);
	}
	catch (const std::exception& ex)
	{
		PLOGE << ex.what();
	}
	catch (...)
	{
		PLOGE << "unknown error";
	}

	return {};
}

QString Fb2InpxParser::GetSeqNumber(QString seqNumber)
{
	bool ok = false;
	if (const auto value = seqNumber.toInt(&ok); ok && value > 0)
		return seqNumber;
	return QString {};
}
