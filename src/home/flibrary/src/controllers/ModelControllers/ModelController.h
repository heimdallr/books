#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "models/IModelObserver.h"
#include "ModelControllerType.h"

class QAbstractItemModel;

namespace HomeCompa {
class Settings;
}

namespace HomeCompa::Flibrary {

class IModelControllerObserver;
class ModelControllerSettings;
struct Book;

class ModelController
	: public QObject
	, virtual public IModelObserver
{
	NON_COPY_MOVABLE(ModelController)
	Q_OBJECT

	Q_PROPERTY(int currentIndex READ GetCurrentLocalIndex WRITE UpdateCurrentIndex NOTIFY CurrentIndexChanged)
	Q_PROPERTY(QString viewMode READ GetViewMode WRITE SetViewMode NOTIFY ViewModeChanged)
	Q_PROPERTY(QString viewModeValue READ GetViewModeValue WRITE SetViewModeValue)
	Q_PROPERTY(bool focused READ GetFocused NOTIFY FocusedChanged)
	Q_PROPERTY(int count READ GetCount NOTIFY CountChanged)

public:
	Q_INVOKABLE void SetPageSize(int pageSize);
	Q_INVOKABLE QAbstractItemModel * GetModel();
	Q_INVOKABLE void OnKeyPressed(int key, int modifiers);
	Q_INVOKABLE QString GetViewSource() const;

signals:
	void CurrentIndexChanged() const;
	void FocusedChanged() const;
	void CountChanged() const;
	void ViewModeChanged() const;

public:
	ModelController(std::unique_ptr<ModelControllerSettings> modelControllerSettings
		, const char * typeName
		, const char * sourceName
		, QObject * parent = nullptr
	);
	~ModelController() override;

	void RegisterObserver(IModelControllerObserver * observer);
	void UnregisterObserver(IModelControllerObserver * observer);

	void SetFocused(bool value);
	QAbstractItemModel * GetCurrentModel();
	QString GetId(int index);

public:
	virtual ModelControllerType GetType() const noexcept = 0;
	virtual void UpdateModelData() = 0;

private: // ModelObserver
	void HandleModelItemFound(int index) override;
	void HandleItemClicked(int index) override;
	void HandleItemDoubleClicked(int index) override;
	void HandleInvalidated() override;

private: // property getters
	bool GetFocused() const noexcept;
	int GetCurrentLocalIndex();
	QString GetViewMode() const;
	QString GetViewModeValue() const;
	int GetCount() const;

private: // property setters
	void UpdateCurrentIndex(int globalIndex);
	void SetViewMode(const QString & viewMode);
	void SetViewModeValue(const QString & text);

protected:
	void FindCurrentItem();
	void SaveCurrentItemId();
	static const char * GetTypeName(ModelControllerType type);

	virtual QAbstractItemModel * CreateModel() = 0;
	virtual bool SetCurrentIndex(int index);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
