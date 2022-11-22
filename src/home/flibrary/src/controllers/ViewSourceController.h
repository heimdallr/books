#pragma once

#include <QObject>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "models/SimpleModel.h"

class QVariant;

namespace HomeCompa {
class Settings;
}

namespace HomeCompa::Flibrary {

class ViewSourceController final
	: public QObject
{
	NON_COPY_MOVABLE(ViewSourceController)
	Q_OBJECT
	Q_PROPERTY(QString title READ GetTitle NOTIFY TitleChanged)
	Q_PROPERTY(QString value READ GetValue WRITE SetValue NOTIFY ValueChanged)

signals:
	void TitleChanged() const;
	void ValueChanged() const;

public:
	ViewSourceController(Settings & uiSetting, const char * keyName, const QVariant & defaultValue, SimpleModelItems simpleModelItems, QObject * parent = nullptr);
	~ViewSourceController() override;

	Q_INVOKABLE QAbstractItemModel * GetModel() const;

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
