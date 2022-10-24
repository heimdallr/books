#include <QSettings>

#include "Settings.h"

namespace HomeCompa {

struct Settings::Impl
{
	Impl(const QString & organization, const QString & application)
		: settings(organization, application)
	{
	}

	QSettings settings;
};

Settings::Settings(const QString & organization, const QString & application)
	: m_impl(organization, application)
{
}

Settings::~Settings() = default;

QVariant Settings::Get(const QString & key, const QVariant & defaultValue) const
{
	return m_impl->settings.value(key, defaultValue);
}

void Settings::Set(const QString & key, const QVariant & value)
{
	m_impl->settings.setValue(key, value);
}

}
