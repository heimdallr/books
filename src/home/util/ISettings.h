#pragma once

#include <QStringList>
#include <QVariant>

namespace HomeCompa {

class ISettingsObserver;

class ISettings
{
public:
	virtual ~ISettings() = default;

public:
	virtual QVariant Get(const QString & key, const QVariant & defaultValue = {}) const = 0;
	virtual void Set(const QString & key, const QVariant & value) = 0;

	virtual bool HasKey(const QString & key) const = 0;
	virtual bool HasGroup(const QString & group) const = 0;

	virtual QStringList GetKeys() const = 0;
	virtual QStringList GetGroups() const = 0;

	virtual void Remove(const QString & key) = 0;

	virtual void BeginGroup(const QString & group) = 0;
	virtual void EndGroup() = 0;

	virtual void RegisterObserver(ISettingsObserver * observer) = 0;
	virtual void UnregisterObserver(ISettingsObserver * observer) = 0;
};

class SettingsGroup
{
public:
	SettingsGroup(ISettings & settings, const QString & group)
		: m_settings(settings)
	{
		m_settings.BeginGroup(group);
	}
	~SettingsGroup()
	{
		m_settings.EndGroup();
	}

private:
	ISettings & m_settings;
};
}
