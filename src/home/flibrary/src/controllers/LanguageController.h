#pragma once

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

	BooksModelControllerObserver * GetBooksModelControllerObserver() noexcept;
	ComboBoxController * GetComboBoxController() noexcept;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
