#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>
#include <QVariant>

#include "util/Settings.h"

#include "LocaleController.h"

#include "Resources/flibrary_locales.h"
#include "Settings/UiSettings_keys.h"

namespace HomeCompa::Flibrary {

struct LocaleController::Impl
{
	explicit Impl(Settings & uiSettings_)
		: uiSettings(uiSettings_)
	{
	}

	Settings & uiSettings;
	QTranslator translator;
};

LocaleController::LocaleController(Settings & uiSettings, QObject * parent)
	: QObject(parent)
	, m_impl(uiSettings)
{
	m_impl->translator.load(QString(":/resources/%1.qm").arg(GetLocale()));
	QCoreApplication::installTranslator(&m_impl->translator);
}

LocaleController::~LocaleController() = default;

QStringList LocaleController::GetLocales() const
{
	QStringList result;
	result.reserve(static_cast<int>(std::size(LOCALES)));
	std::ranges::copy(LOCALES, std::back_inserter(result));
	return result;
}

QString LocaleController::GetLocale() const
{
	auto locale = m_impl->uiSettings.Get(Constant::UiSettings_ns::locale).toString();
	if (!locale.isEmpty())
		return locale;

	if (const auto it = std::ranges::find_if(LOCALES, [sysLocale = QLocale::system().name()](const char * item){ return sysLocale.startsWith(item); }); it != std::cend(LOCALES))
		return *it;

	assert(!std::empty(LOCALES));
	return LOCALES[0];
}

void LocaleController::SetLocale(const QString & locale)
{
	m_impl->uiSettings.Set(Constant::UiSettings_ns::locale, locale);
	emit LocaleChanged();
}

}
