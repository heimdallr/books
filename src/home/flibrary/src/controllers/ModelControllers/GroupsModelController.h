#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QAbstractItemModel;

namespace HomeCompa::Util {
class IExecutor;
}

namespace HomeCompa::DB {
class IDatabase;
}

namespace HomeCompa::Flibrary {

struct Book;

class GroupsModelController
	: public QObject
{
	NON_COPY_MOVABLE(GroupsModelController)
	Q_OBJECT

	Q_PROPERTY(bool checkNewNameInProgress READ IsCheckNewNameInProgress NOTIFY CheckNewNameInProgressChanged)
	Q_PROPERTY(bool toAddExists READ IsToAddExists NOTIFY ToAddExistsChanged)
	Q_PROPERTY(QString errorText READ GetErrorText NOTIFY ErrorTextChanged)

signals:
	void CheckNewNameInProgressChanged() const;
	void ErrorTextChanged() const;
	void ToAddExistsChanged() const;

	void GetCheckedBooksRequest(std::vector<Book> & books) const;

public:
	Q_INVOKABLE QAbstractItemModel * GetAddToModel();
	Q_INVOKABLE QAbstractItemModel * GetRemoveFromModel();
	Q_INVOKABLE void Reset(long long bookId);
	Q_INVOKABLE void AddTo(const QString & id);
	Q_INVOKABLE void AddToNew(const QString & name);
	Q_INVOKABLE void RemoveFrom(const QString & id);
	Q_INVOKABLE void RemoveFromAll();
	Q_INVOKABLE void CheckNewName(const QString & name);
	Q_INVOKABLE void CreateNewGroup(const QString & name);
	Q_INVOKABLE void RemoveGroup(long long groupId);

public:
	GroupsModelController(Util::IExecutor & executor, DB::IDatabase & db, QObject * parent = nullptr);
	~GroupsModelController() override;

private: // property getters
	bool IsCheckNewNameInProgress() const noexcept;
	bool IsToAddExists() const noexcept;
	const QString & GetErrorText() const noexcept;

private: // property setters

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
