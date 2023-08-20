#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "logicLib.h"

class QAbstractItemModel;
class QObject;

namespace HomeCompa::Flibrary {

QAbstractItemModel * CreateLogModel(QObject * parent = nullptr);

class LOGIC_API LogModelAppender
{
	NON_COPY_MOVABLE(LogModelAppender)

public:
	LogModelAppender();
	~LogModelAppender();

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
