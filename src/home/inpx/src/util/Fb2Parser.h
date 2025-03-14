#pragma once

#include <QString>
#include <QStringList>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

class QIODevice;

namespace HomeCompa::Inpx
{

class Fb2Parser
{
	NON_COPY_MOVABLE(Fb2Parser)

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

		Authors authors;
		QStringList genres;
		QString title;
		QString lang;
		QString series;
		QString keywords;
		int seqNumber { -1 };

		QString error;
	};

public:
	explicit Fb2Parser(QIODevice& stream, const QString& fileName);
	~Fb2Parser();

	Data Parse();

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Inpx
