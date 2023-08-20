#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "ISettingsProvider.h"

namespace HomeCompa {
class Settings;
}

namespace HomeCompa::Flibrary {

class IBooksModelControllerObserver;

class LanguageController
	: public QObject
{
	NON_COPY_MOVABLE(LanguageController)

public:
	class ILanguageProvider
		: virtual public ISettingsProvider
	{
	public:
		virtual QStringList GetLanguages() = 0;
		virtual QString GetLanguage() = 0;
		virtual void SetLanguage(const QString & language) = 0;
	};

public:
	explicit LanguageController(ILanguageProvider & languageProvider, QObject * parent = nullptr);
	~LanguageController() override;

	IBooksModelControllerObserver * GetBooksModelControllerObserver() noexcept;
	ComboBoxController * GetComboBoxController() noexcept;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
