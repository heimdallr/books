#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>
#include <QVariant>

#include "util/Settings.h"

#include "ModelControllers/BooksModelControllerObserver.h"

#include "LocaleController.h"

#include "Resources/flibrary.h"

namespace HomeCompa::Flibrary {

namespace {

constexpr auto LANGUAGE = "Language";

}

struct LocaleController::Impl
	: BooksModelControllerObserver
{
	Impl(LocaleController & self, LanguageProvider & languageProvider_)
		: languageProvider(languageProvider_)
		, m_self(self)
	{
	}

	LanguageProvider & languageProvider;
	QTranslator translator;
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
	LocaleController & m_self;
};

LocaleController::LocaleController(LanguageProvider & languageProvider, QObject * parent)
	: QObject(parent)
	, m_impl(*this, languageProvider)
{
	m_impl->translator.load(QString(":/resources/%1.qm").arg(GetLocale()));
	QCoreApplication::installTranslator(&m_impl->translator);
}

LocaleController::~LocaleController() = default;

BooksModelControllerObserver * LocaleController::GetBooksModelControllerObserver()
{
	return m_impl.get();
}

QStringList LocaleController::GetLanguages()
{
	return m_impl->languageProvider.GetLanguages();
}

QStringList LocaleController::GetLocales() const
{
	QStringList result;
	result.reserve(static_cast<int>(std::size(LOCALES)));
	std::ranges::copy(LOCALES, std::back_inserter(result));
	return result;
}

QString LocaleController::GetLanguage()
{
	return m_impl->languageProvider.GetLanguage();
}

QString LocaleController::GetLocale() const
{
	auto locale = m_impl->languageProvider.GetSettings().Get(LANGUAGE, "").toString();
	if (!locale.isEmpty())
		return locale;

	if (const auto it = std::ranges::find_if(LOCALES, [sysLocale = QLocale::system().name()](const char * item){ return sysLocale.startsWith(item); }); it != std::cend(LOCALES))
		return *it;

	assert(!std::empty(LOCALES));
	return LOCALES[0];
}

void LocaleController::SetLanguage(const QString & language)
{
	if (!m_impl->preventSetLanguageFilter)
		m_impl->languageProvider.SetLanguage(language);
}

void LocaleController::SetLocale(const QString & locale)
{
	m_impl->languageProvider.GetSettings().Set(LANGUAGE, locale);
	emit LocaleChanged();
}

}
