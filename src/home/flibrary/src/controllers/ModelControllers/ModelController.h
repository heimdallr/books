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
	Q_PROPERTY(int currentIndex READ GetCurrentLocalIndex WRITE SetCurrentLocalIndex NOTIFY CurrentIndexChanged)
	Q_PROPERTY(QString viewMode READ GetViewMode CONSTANT)
	Q_PROPERTY(bool focused READ GetFocused NOTIFY FocusedChanged)
	Q_PROPERTY(QString navigationType READ GetNavigationType NOTIFY NavigationTypeChanged)

public:
	Q_INVOKABLE void SetViewMode(const QString & mode, const QString & text);
	Q_INVOKABLE void SetPageSize(int pageSize);
	Q_INVOKABLE QAbstractItemModel * GetModel(const QString & modelType);
	Q_INVOKABLE void OnKeyPressed(int key, int modifiers);

public:
	enum class Type
	{
		Navigation,
		Books,
	};

public:
	explicit ModelController(QObject * parent = nullptr);
	~ModelController() override;

	void RegisterObserver(ModelControllerObserver * observer);
	void UnregisterObserver(ModelControllerObserver * observer);

	void SetFocused(bool value);
	QAbstractItemModel * GetCurrentModel();
	const QString & GetCurrentModelType() const;
	int GetId(int index);

public:
	virtual Type GetType() const noexcept = 0;

signals:
	void CurrentIndexChanged() const;
	void FocusedChanged() const;
	void NavigationTypeChanged() const;

private: // property getters
	bool GetFocused() const noexcept;
	int GetCurrentLocalIndex();
	QString GetViewMode() const;

private: // property setters
	void SetCurrentLocalIndex(int);

private:
	virtual QAbstractItemModel * GetModelImpl(const QString & modelType) = 0;
	virtual const QString & GetNavigationType() const;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
