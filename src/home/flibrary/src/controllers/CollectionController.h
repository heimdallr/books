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

signals:
	void CurrentCollectionIdChanged() const;
	void ErrorChanged() const;

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
	Q_INVOKABLE bool AddCollection(QString name, QString db, QString folder);
	Q_INVOKABLE bool CreateCollection(QString name, QString db, QString folder);
	Q_INVOKABLE QAbstractItemModel * GetModel();
	Q_INVOKABLE void RemoveCurrentCollection();

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
