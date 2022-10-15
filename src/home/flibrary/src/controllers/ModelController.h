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
//	NON_COPY_MOVABLE(ModelController)
	Q_OBJECT
	Q_PROPERTY(int currentIndex READ GetCurrentIndex WRITE OnClicked NOTIFY CurrentIndexChanged)
	Q_PROPERTY(QAbstractItemModel * model READ GetModel NOTIFY ModelChanged)
	Q_PROPERTY(QString viewMode READ GetViewMode CONSTANT)

public:
	Q_INVOKABLE void SetViewMode(const QString & mode, const QString & text);

public:
	explicit ModelController(QObject * parent = nullptr);
	~ModelController() override;

	void OnKeyPressed(int key, int modifiers);

	void RegisterObserver(ModelControllerObserver * observer);
	void UnregisterObserver(ModelControllerObserver * observer);

signals:
	void CurrentIndexChanged() const;
	void ModelChanged() const;

protected:
	void SetModel(QAbstractItemModel * model);
	void SetCurrentIndex(int index);

private: // property getters
	int GetCurrentIndex() const;
	QAbstractItemModel * GetModel();
	QString GetViewMode() const;

private: // property setters
	void OnClicked(int index);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
