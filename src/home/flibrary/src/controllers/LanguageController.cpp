#include <QLocale>

#include "util/Settings.h"

#include "ModelControllers/BooksModelControllerObserver.h"

#include "LanguageController.h"

namespace HomeCompa::Flibrary {

struct LanguageController::Impl
	: BooksModelControllerObserver
{
	Impl(LanguageController & self, LanguageProvider & languageProvider_)
		: languageProvider(languageProvider_)
		, m_self(self)
	{
	}

	LanguageProvider & languageProvider;
	bool preventSetLanguageFilter { false };

private: //BooksModelControllerObserver
	void HandleBookChanged(const std::string & /*folder*/, const std::string & /*file*/) override
	{
	}

	void HandleModelReset() override
	{
		preventSetLanguageFilter = true;
		emit m_self.LanguagesChanged();
		preventSetLanguageFilter = false;
	}

private:
	LanguageController & m_self;
};

LanguageController::LanguageController(LanguageProvider & languageProvider, QObject * parent)
	: QObject(parent)
	, m_impl(*this, languageProvider)
{
}

LanguageController::~LanguageController() = default;

BooksModelControllerObserver * LanguageController::GetBooksModelControllerObserver()
{
	return m_impl.get();
}

QStringList LanguageController::GetLanguages()
{
	return m_impl->languageProvider.GetLanguages();
}

QString LanguageController::GetLanguage()
{
	return m_impl->languageProvider.GetLanguage();
}

void LanguageController::SetLanguage(const QString & language)
{
	if (!m_impl->preventSetLanguageFilter)
		m_impl->languageProvider.SetLanguage(language);
}

}
