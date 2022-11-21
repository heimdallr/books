#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "models/ModelObserver.h"

class QAbstractItemModel;

namespace HomeCompa {
class Settings;
}

namespace HomeCompa::DB {
class Database;
}

namespace HomeCompa::Flibrary {

class ModelControllerObserver;
struct Book;

class ModelController
	: public QObject
	, virtual public ModelObserver
{
	NON_COPY_MOVABLE(ModelController)
	Q_OBJECT

	Q_PROPERTY(int currentIndex READ GetCurrentLocalIndex WRITE UpdateCurrentIndex NOTIFY CurrentIndexChanged)
	Q_PROPERTY(QString viewMode READ GetViewMode WRITE SetViewMode NOTIFY ViewModeChanged)
	Q_PROPERTY(bool focused READ GetFocused NOTIFY FocusedChanged)
	Q_PROPERTY(int count READ GetCount NOTIFY CountChanged)

public:
	Q_INVOKABLE void SetViewModeValue(const QString & text);
	Q_INVOKABLE void SetPageSize(int pageSize);
	Q_INVOKABLE QAbstractItemModel * GetModel();
	Q_INVOKABLE void OnKeyPressed(int key, int modifiers);

signals:
	void CurrentIndexChanged() const;
	void FocusedChanged() const;
	void CountChanged() const;
	void ViewModeChanged() const;

public:
	enum class Type
	{
		Navigation,
		Books,
	};

public:
	ModelController(Settings & uiSettings, const char * viewModeKey, const QVariant & viewModeDefaultValue, QObject * parent = nullptr);
	~ModelController() override;

	void RegisterObserver(ModelControllerObserver * observer);
	void UnregisterObserver(ModelControllerObserver * observer);

	void SetFocused(bool value);
	QAbstractItemModel * GetCurrentModel();
	QString GetId(int index);

public:
	virtual Type GetType() const noexcept = 0;

private: // ModelObserver
	void HandleModelItemFound(int index) override;
	void HandleItemClicked(int index) override;
	void HandleInvalidated() override;

private: // property getters
	bool GetFocused() const noexcept;
	int GetCurrentLocalIndex();
	QString GetViewMode() const;
	int GetCount() const;

private: // property setters
	void UpdateCurrentIndex(int globalIndex);
	void SetViewMode(const QString & viewMode);

protected:
	virtual QAbstractItemModel * CreateModel() = 0;
	virtual bool SetCurrentIndex(int index);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
