#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "SettingsProvider.h"

namespace HomeCompa {
class Settings;
}

namespace HomeCompa::Flibrary {

class BooksModelControllerObserver;

class LocaleController
	: public QObject
{
	NON_COPY_MOVABLE(LocaleController)
	Q_OBJECT

	Q_PROPERTY(QStringList languages READ GetLanguages NOTIFY LanguagesChanged)
	Q_PROPERTY(QStringList locales READ GetLocales CONSTANT)
	Q_PROPERTY(QString language READ GetLanguage WRITE SetLanguage)
	Q_PROPERTY(QString locale READ GetLocale WRITE SetLocale NOTIFY LocaleChanged)

signals:
	void LanguagesChanged() const;
	void LocaleChanged() const;

public:
	class LanguageProvider
		: virtual public SettingsProvider
	{
	public:
		virtual QStringList GetLanguages() = 0;
		virtual QString GetLanguage() = 0;
		virtual void SetLanguage(const QString & language) = 0;
	};

public:
	explicit LocaleController(LanguageProvider & languageProvider, QObject * parent = nullptr);
	~LocaleController() override;

	BooksModelControllerObserver * GetBooksModelControllerObserver();

private: // property getters
	QString GetLanguage();
	QString GetLocale() const;
	QStringList GetLanguages();
	QStringList GetLocales() const;

private: // property setters
	void SetLanguage(const QString & language);
	void SetLocale(const QString & locale);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
