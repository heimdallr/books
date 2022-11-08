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
	m_impl->settings.sync();
}

bool Settings::HasKey(const QString & key) const
{
	return m_impl->settings.contains(key);
}

bool Settings::HasGroup(const QString & group) const
{
	return GetGroups().contains(group);
}

QStringList Settings::GetKeys() const
{
	return m_impl->settings.childKeys();
}

QStringList Settings::GetGroups() const
{
	return m_impl->settings.childGroups();
}

void Settings::Remove(const QString & key)
{
	m_impl->settings.remove(key);
	m_impl->settings.sync();
}

void Settings::BeginGroup(const QString & group)
{
	m_impl->settings.beginGroup(group);
}

void Settings::EndGroup()
{
	m_impl->settings.endGroup();
}

SettingsGroup::SettingsGroup(Settings & settings, const QString & group)
	: m_settings(settings)
{
	m_settings.BeginGroup(group);
}

SettingsGroup::~SettingsGroup()
{
	m_settings.EndGroup();
}

}
