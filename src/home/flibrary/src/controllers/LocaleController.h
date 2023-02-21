#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa {
class Settings;
}

namespace HomeCompa::Flibrary {

class IBooksModelControllerObserver;

class LocaleController
	: public QObject
{
	NON_COPY_MOVABLE(LocaleController)
	Q_OBJECT

	Q_PROPERTY(QStringList locales READ GetLocales CONSTANT)
	Q_PROPERTY(QString locale READ GetLocale WRITE SetLocale NOTIFY LocaleChanged)

signals:
	void LocaleChanged() const;

public:
	explicit LocaleController(Settings & uiSettings, QObject * parent = nullptr);
	~LocaleController() override;

private: // property getters
	QString GetLocale() const;
	QStringList GetLocales() const;

private: // property setters
	void SetLocale(const QString & locale);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
