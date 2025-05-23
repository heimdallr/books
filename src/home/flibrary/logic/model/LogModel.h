#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "export/logic.h"

class QAbstractItemModel;
class QObject;

namespace HomeCompa::Flibrary
{

QAbstractItemModel* CreateLogModel(QObject* parent = nullptr);

class LOGIC_EXPORT LogModelAppender
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
