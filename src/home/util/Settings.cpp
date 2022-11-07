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

QStringList Settings::GetKeys() const
{
	return m_impl->settings.childKeys();
}

QStringList Settings::GetGroups() const
{
	return m_impl->settings.childGroups();
}

bool Settings::HasKey(const QString & key) const
{
	return m_impl->settings.contains(key);
}

bool Settings::HasGroup(const QString & group) const
{
	return GetGroups().contains(group);
}

SettingsGroup::SettingsGroup(Settings & settings, const QString & group)
	: m_settings(settings)
{
	m_settings.m_impl->settings.beginGroup(group);
}

SettingsGroup::~SettingsGroup()
{
	m_settings.m_impl->settings.endGroup();
}

}
