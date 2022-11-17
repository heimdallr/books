#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "SettingsProvider.h"

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

struct Collection;

class CollectionController
	: public QObject
{
	NON_COPY_MOVABLE(CollectionController)
	Q_OBJECT

	Q_PROPERTY(QString currentCollectionId READ GetCurrentCollectionId WRITE SetCurrentCollectionId NOTIFY CurrentCollectionIdChanged)
	Q_PROPERTY(QString error READ GetError WRITE SetError NOTIFY ErrorChanged)
	Q_PROPERTY(bool addMode READ GetAddMode WRITE SetAddMode NOTIFY AddModeChanged)
	Q_PROPERTY(bool hasUpdate READ HasUpdate WRITE SetHasUpdate NOTIFY HasUpdateChanged)


signals:
	void AddModeChanged() const;
	void CurrentCollectionIdChanged() const;
	void ErrorChanged() const;
	void HasUpdateChanged() const;

	void ShowLog(bool) const;

public:
	class Observer
		: virtual public SettingsProvider
	{
	public:
		virtual void HandleCurrentCollectionChanged(const Collection & collection) = 0;
	};

public:
	explicit CollectionController(Observer & observer, QObject * parent = nullptr);
	~CollectionController() override;

public:
	void CheckForUpdate(const Collection & collection);

public:
	Q_INVOKABLE bool AddCollection(QString name, QString db, QString folder);
	Q_INVOKABLE bool CreateCollection(QString name, QString db, QString folder);
	Q_INVOKABLE QAbstractItemModel * GetModel();
	Q_INVOKABLE void ApplyUpdate();
	Q_INVOKABLE void DiscardUpdate();
	Q_INVOKABLE void RemoveCurrentCollection();

private: // property getters
	bool GetAddMode() const noexcept;
	bool HasUpdate() const noexcept;
	const QString & GetCurrentCollectionId() const noexcept;
	const QString & GetError() const noexcept;

private: // property setters
	void SetAddMode(bool value);
	void SetHasUpdate(bool value);
	void SetCurrentCollectionId(const QString & id);
	void SetError(const QString & error);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
