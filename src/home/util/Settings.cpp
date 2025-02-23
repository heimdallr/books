#include "Settings.h"

#include <QSettings>

#include "fnd/observable.h"

#include "ISettingsObserver.h"

namespace HomeCompa
{

struct Settings::Impl final : Observable<ISettingsObserver>
{
	explicit Impl(const QString& fileName)
		: settings(fileName, QSettings::Format::IniFormat)
	{
	}

	Impl(const QString& organization, const QString& application)
		: settings(QSettings::NativeFormat, QSettings::UserScope, organization, application)
	{
	}

	QString Key(const QString& key) const
	{
		return group.back() + (group.back().isEmpty() ? "" : "/") + key;
	}

	QSettings settings;
	std::vector<QString> group { 1 };
};

Settings::Settings(const QString& fileName)
	: m_impl(std::make_unique<Impl>(fileName))
{
}

Settings::Settings(const QString& organization, const QString& application)
	: m_impl(std::make_unique<Impl>(organization, application))
{
}

Settings::~Settings() = default;

QVariant Settings::Get(const QString& key, const QVariant& defaultValue) const
{
	return m_impl->settings.value(key, defaultValue);
}

void Settings::Set(const QString& key, const QVariant& value)
{
	if (Get(key) == value)
		return;

	m_impl->settings.setValue(key, value);
	m_impl->settings.sync();

	m_impl->Perform(&ISettingsObserver::HandleValueChanged, m_impl->Key(key), std::cref(value));
}

bool Settings::HasKey(const QString& key) const
{
	return m_impl->settings.contains(key);
}

bool Settings::HasGroup(const QString& group) const
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

void Settings::Remove(const QString& key)
{
	m_impl->settings.remove(key);
	m_impl->settings.sync();
}

void Settings::RegisterObserver(ISettingsObserver* observer)
{
	m_impl->Register(observer);
}

void Settings::UnregisterObserver(ISettingsObserver* observer)
{
	m_impl->Unregister(observer);
}

void Settings::BeginGroup(const QString& group) const
{
	m_impl->settings.beginGroup(group);
	m_impl->group.push_back(m_impl->Key(group));
}

void Settings::EndGroup() const
{
	m_impl->settings.endGroup();
	m_impl->group.pop_back();
}

} // namespace HomeCompa
