#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "fnd/observer.h"

#include "models/SimpleModel.h"

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class ComboBoxDataProvider
{
public:
	virtual ~ComboBoxDataProvider() = default;
	virtual QString GetValue() const = 0;
};

class ComboBoxObserver : public Observer
{
public:
	virtual void SetValue(const QString & value) = 0;
};

class ComboBoxController final
	: public QObject
{
	NON_COPY_MOVABLE(ComboBoxController)
	Q_OBJECT
	Q_PROPERTY(QString title READ GetTitle NOTIFY TitleChanged)
	Q_PROPERTY(QString value READ GetValue WRITE SetValue NOTIFY ValueChanged)

signals:
	void TitleChanged() const;
	void ValueChanged() const;

public:
	explicit ComboBoxController(ComboBoxDataProvider & dataProvider, QObject * parent = nullptr);
	~ComboBoxController() override;

public:
	Q_INVOKABLE QAbstractItemModel * GetModel() const;

public:
	void SetData(SimpleModelItems simpleModelItems);
	void OnValueChanged() const;

	void RegisterObserver(ComboBoxObserver * observer);
	void UnregisterObserver(ComboBoxObserver * observer);

private: // property getters
	const QString & GetTitle() const;
	QString GetValue() const;

private: // property setters
	void SetValue(const QString & value);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
