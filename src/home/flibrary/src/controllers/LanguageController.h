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

class LanguageController
	: public QObject
{
	NON_COPY_MOVABLE(LanguageController)
	Q_OBJECT

	Q_PROPERTY(QStringList languages READ GetLanguages NOTIFY LanguagesChanged)
	Q_PROPERTY(QString language READ GetLanguage WRITE SetLanguage)

signals:
	void LanguagesChanged() const;

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
	explicit LanguageController(LanguageProvider & languageProvider, QObject * parent = nullptr);
	~LanguageController() override;

	BooksModelControllerObserver * GetBooksModelControllerObserver();

private: // property getters
	QString GetLanguage();
	QStringList GetLanguages();

private: // property setters
	void SetLanguage(const QString & language);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
