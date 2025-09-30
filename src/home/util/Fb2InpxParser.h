#pragma once

#include <QStringList>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "export/Util.h"

namespace HomeCompa
{
class Zip;
}

class QDateTime;
class QIODevice;

namespace HomeCompa::Util
{

class Fb2InpxParser
{
	NON_COPY_MOVABLE(Fb2InpxParser)

public:
	static constexpr wchar_t LIST_SEPARATOR   = ':';
	static constexpr wchar_t NAMES_SEPARATOR  = ',';
	static constexpr wchar_t FIELDS_SEPARATOR = '\x04';

public:
	struct Data
	{
		struct Author
		{
			QString first;
			QString last;
			QString middle;
		};

		using Authors = std::vector<Author>;

		Authors     authors;
		QStringList genres;
		QString     title;
		QString     lang;
		QString     series;
		QString     keywords;
		QString     seqNumber;
		QString     year;

		QString error;
	};

public:
	UTIL_EXPORT static QString Parse(const QString& folder, const Zip& zip, const QString& fileName, const QDateTime& zipDateTime, bool isDeleted);
	UTIL_EXPORT static QString GetSeqNumber(QString seqNumber);

private:
	Fb2InpxParser(QIODevice& stream, const QString& fileName);
	~Fb2InpxParser();

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Util
