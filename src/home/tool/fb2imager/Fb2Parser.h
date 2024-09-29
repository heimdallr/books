#pragma once

#include <QString>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QIODevice;

namespace HomeCompa::fb2imager {

class Fb2Parser
{
	NON_COPY_MOVABLE(Fb2Parser)

public:
	struct Data
	{
		QString title;
		QString lang;
		std::vector<QString> genres;

		QString error;
	};

	using OnBinaryFound = std::function<bool(const QString&, const QByteArray&)>;

public:
	Fb2Parser(QIODevice & stream, QString fileName, OnBinaryFound binaryCallback);
	~Fb2Parser();

	Data Parse();

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
