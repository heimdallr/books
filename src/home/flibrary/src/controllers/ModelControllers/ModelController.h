#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QAbstractItemModel;

namespace HomeCompa::DB {
class Database;
}

namespace HomeCompa::Flibrary {

class ModelControllerObserver;

class ModelController
	: public QObject
{
	NON_COPY_MOVABLE(ModelController)
	Q_OBJECT
	Q_PROPERTY(int currentIndex READ GetCurrentLocalIndex NOTIFY CurrentIndexChanged)
	Q_PROPERTY(QString viewMode READ GetViewMode CONSTANT)

public:
	Q_INVOKABLE void SetViewMode(const QString & mode, const QString & text);
	Q_INVOKABLE void SetPageSize(int pageSize);
	Q_INVOKABLE QAbstractItemModel * GetModel();

public:
	enum class Type
	{
		Authors,
		Books,
	};

public:
	explicit ModelController(QAbstractItemModel * model, QObject * parent = nullptr);
	~ModelController() override;

	void OnKeyPressed(int key, int modifiers);

	void RegisterObserver(ModelControllerObserver * observer);
	void UnregisterObserver(ModelControllerObserver * observer);

public:
	virtual Type GetType() const noexcept = 0;

signals:
	void CurrentIndexChanged() const;

private: // property getters
	int GetCurrentLocalIndex();
	QString GetViewMode() const;

private: // property setters

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
