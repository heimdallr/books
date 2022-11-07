#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "SettingsProvider.h"

namespace HomeCompa::Flibrary {

struct Collection;

class CollectionController
	: public QObject
{
	NON_COPY_MOVABLE(CollectionController)
	Q_OBJECT

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
	Q_INVOKABLE void AddCollection(QString name, QString db, QString folder);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
