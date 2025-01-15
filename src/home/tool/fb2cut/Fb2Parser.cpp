#include "Fb2Parser.h"

#include <QTextStream>
#include <QString>
#include <QHash>

#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "common/Constant.h"

#include "util/xml/SaxParser.h"
#include "util/xml/XmlAttributes.h"
#include "util/xml/XmlWriter.h"

using namespace HomeCompa;
using namespace fb2cut;

namespace {

constexpr auto ID = "id";
constexpr auto IMAGE = "image";

constexpr auto GENRE = "FictionBook/description/title-info/genre";
constexpr auto BOOK_TITLE = "FictionBook/description/title-info/book-title";
constexpr auto LANG = "FictionBook/description/title-info/lang";
constexpr auto BINARY = "FictionBook/binary";
constexpr auto COVERPAGE_IMAGE = "FictionBook/description/title-info/coverpage/image";

}

class Fb2Parser::Impl final
	: public Util::SaxParser
{
public:
	explicit Impl(QString fileName, QIODevice & input, QIODevice & output, OnBinaryFound binaryCallback)
		: SaxParser(input, 512)
		, m_fileName(std::move(fileName))
		, m_binaryCallback(std::move(binaryCallback))
		, m_writer(output)
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
	bool OnProcessingInstruction(const QString & target, const QString & data) override
	{
		m_writer.WriteProcessingInstruction(target, data);
		return true;
	}

	bool OnStartElement(const QString & name, const QString & path, const Util::XmlAttributes & attributes) override
	{
		using ParseElementFunction = bool(Impl::*)(const Util::XmlAttributes &);
		using ParseElementItem = std::pair<const char *, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[]
		{
			{ BINARY, &Impl::OnStartElementBinary },
		};

		if (path.compare(BINARY, Qt::CaseInsensitive) == 0)
			return Parse(*this, PARSERS, path, attributes);

		if (name.compare(IMAGE, Qt::CaseInsensitive) == 0)
		{
			m_writer.WriteStartElement(name);
			for (size_t i = 0, sz = attributes.GetCount(); i < sz; ++i)
			{
				const auto keyName = attributes.GetName(i);
				auto image = attributes.GetValue(i);
				if (!keyName.endsWith(":href"))
				{
					m_writer.WriteAttribute(keyName, image);
					continue;
				}

				while (!image.isEmpty() && image.front() == '#')
					image.removeFirst();

				const auto isCover = path.compare(COVERPAGE_IMAGE, Qt::CaseInsensitive) == 0;
				if (isCover)
					m_coverpage = image;

				const auto imageName = m_imageNames.emplace(std::move(image), m_imageNames.size()).first->second;
				m_writer.WriteAttribute(keyName, isCover ? QString("#%1").arg(Global::COVER) : QString("#%1").arg(imageName));
			}
		}
		else
		{
			m_writer.WriteStartElement(name, attributes);
		}

		return Parse(*this, PARSERS, path, attributes);
	}

	bool OnEndElement(const QString &, const QString & path) override
	{
		if (path.compare(BINARY, Qt::CaseInsensitive) == 0)
			return true;

		m_writer.WriteEndElement();

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

		if (path.compare(BINARY, Qt::CaseInsensitive))
			m_writer.WriteCharacters(value);

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
		const auto it = m_imageNames.find(m_binaryId);
		if (it != m_imageNames.end())
			m_binaryCallback(QString::number(it->second), m_binaryId.compare(m_coverpage, Qt::CaseInsensitive) == 0, QByteArray::fromBase64(value.toUtf8()));

		return true;
	}

private:
	const QString m_fileName;
	const OnBinaryFound m_binaryCallback;
	Util::XmlWriter m_writer;
	QString m_binaryId;
	QString m_coverpage;
	Data m_data {};
	std::unordered_map<QString, size_t, std::hash<QString>, std::equal_to<>> m_imageNames;
};

Fb2Parser::Fb2Parser(QString fileName, QIODevice & input, QIODevice & output, OnBinaryFound binaryCallback)
	: m_impl(std::move(fileName), input, output, std::move(binaryCallback))
{
}

Fb2Parser::~Fb2Parser() = default;

Fb2Parser::Data Fb2Parser::Parse()
{
	return m_impl->GetData();
}
