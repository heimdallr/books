#pragma once

#include <QString>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

class QIODevice;

namespace HomeCompa::fb2cut
{

class Fb2ImageParser
{
	NON_COPY_MOVABLE(Fb2ImageParser)

public:
	using OnBinaryFound = std::function<void(QString&&, bool isCover, const QByteArray& data)>;

	static void Parse(QIODevice& input, OnBinaryFound binaryCallback);

private:
	Fb2ImageParser(QIODevice& input, OnBinaryFound binaryCallback);
	~Fb2ImageParser();

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

class Fb2Parser
{
	NON_COPY_MOVABLE(Fb2Parser)

public:
	static void Parse(QString fileName, QIODevice& input, QIODevice& output, const std::unordered_map<QString, int>& replaceId);

private:
	Fb2Parser(QString fileName, QIODevice& input, QIODevice& output, const std::unordered_map<QString, int>& replaceId);
	~Fb2Parser();

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::fb2cut
