#pragma once

#include <QString>
#include <QStringList>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QIODevice;

namespace HomeCompa::Inpx {

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
		QString date;
		QString lang;
		QString series;
		int seqNumber { -1 };
	};

public:
	explicit Fb2Parser(QByteArray data);
	~Fb2Parser();

	Data Parse(const QString & fileName);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
