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
	Q_PROPERTY(int currentIndex READ GetCurrentLocalIndex WRITE UpdateCurrentIndex NOTIFY CurrentIndexChanged)
	Q_PROPERTY(QString viewMode READ GetViewMode CONSTANT)
	Q_PROPERTY(bool focused READ GetFocused NOTIFY FocusedChanged)

public:
	Q_INVOKABLE void SetViewMode(const QString & mode, const QString & text);
	Q_INVOKABLE void SetPageSize(int pageSize);
	Q_INVOKABLE QAbstractItemModel * GetModel();
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
	QString GetId(int index);

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
	void UpdateCurrentIndex(int globalIndex);

protected:
	virtual QAbstractItemModel * CreateModel() = 0;
	virtual bool SetCurrentIndex(int index);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
