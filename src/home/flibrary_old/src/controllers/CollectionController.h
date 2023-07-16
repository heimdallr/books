#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "ISettingsProvider.h"

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class DialogController;
struct Collection;

class CollectionController
	: public QObject
{
	NON_COPY_MOVABLE(CollectionController)
	Q_OBJECT

	Q_PROPERTY(QString currentCollectionId READ GetCurrentCollectionId WRITE SetCurrentCollectionId NOTIFY CurrentCollectionIdChanged)
	Q_PROPERTY(QString error READ GetError WRITE SetError NOTIFY ErrorChanged)

signals:
	void CurrentCollectionIdChanged() const;
	void ErrorChanged() const;

	void ShowLog(bool) const;

public:
	class IObserver
		: virtual public ISettingsProvider
	{
	public:
		virtual void HandleCurrentCollectionChanged(const Collection & collection) = 0;
	};

public:
	explicit CollectionController(IObserver & observer, QObject * parent = nullptr);
	~CollectionController() override;

public:
	void CheckForUpdate(const Collection & collection);

public:
	Q_INVOKABLE bool AddCollection(QString name, QString db, QString folder);
	Q_INVOKABLE void CreateCollection(QString name, QString db, QString folder);
	Q_INVOKABLE bool CheckCreateCollection(const QString & name, const QString & db, const QString & folder);
	Q_INVOKABLE QAbstractItemModel * GetModel();
	Q_INVOKABLE int CollectionsCount() const noexcept;
	Q_INVOKABLE DialogController * GetAddModeDialogController() noexcept;
	Q_INVOKABLE DialogController * GetHasUpdateDialogController() noexcept;
	Q_INVOKABLE DialogController * GetRemoveCollectionConfirmDialogController() noexcept;
	Q_INVOKABLE DialogController * GetRemoveDatabaseConfirmDialogController() noexcept;


private: // property getters
	const QString & GetCurrentCollectionId() const noexcept;
	const QString & GetError() const noexcept;

private: // property setters
	void SetCurrentCollectionId(const QString & id);
	void SetError(const QString & error);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
