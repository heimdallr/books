#pragma once

#include <QString>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

class QIODevice;

namespace HomeCompa::fb2cut
{

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

	using OnBinaryFound = std::function<void(const QString& name, bool isCover, QByteArray data)>;

public:
	Fb2Parser(QString fileName, QIODevice& input, QIODevice& output, OnBinaryFound binaryCallback);
	~Fb2Parser();

	Data Parse();

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::fb2cut
