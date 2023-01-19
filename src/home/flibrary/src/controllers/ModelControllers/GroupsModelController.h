#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QAbstractItemModel;

namespace HomeCompa::Util {
class Executor;
}

namespace HomeCompa::DB {
class Database;
}

namespace HomeCompa::Flibrary {

class GroupsModelController
	: public QObject
{
	NON_COPY_MOVABLE(GroupsModelController)
	Q_OBJECT

public:
	Q_INVOKABLE QAbstractItemModel * GetAddToModel();
	Q_INVOKABLE QAbstractItemModel * GetRemoveFromModel();
	Q_INVOKABLE void Reset(long long bookId);
	Q_INVOKABLE void AddTo(const QString & id);
	Q_INVOKABLE void AddToNew(const QString & name);
	Q_INVOKABLE void RemoveFrom(const QString & id);

public:
	GroupsModelController(Util::Executor & executor, DB::Database & db, QObject * parent = nullptr);
	~GroupsModelController() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
