#pragma once

#include <QStringList>
#include <QVariant>

#include "fnd/NonCopyMovable.h"

namespace HomeCompa {

class ISettingsObserver;

class ISettings  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ISettings() = default;

public:
	[[nodiscard]] virtual QVariant Get(const QString & key, const QVariant & defaultValue = {}) const = 0;
	virtual void Set(const QString & key, const QVariant & value) = 0;

	[[nodiscard]] virtual bool HasKey(const QString & key) const = 0;
	[[nodiscard]] virtual bool HasGroup(const QString & group) const = 0;

	[[nodiscard]] virtual QStringList GetKeys() const = 0;
	[[nodiscard]] virtual QStringList GetGroups() const = 0;

	virtual void Remove(const QString & key) = 0;

	virtual void BeginGroup(const QString & group) const = 0;
	virtual void EndGroup() const = 0;

	virtual void RegisterObserver(ISettingsObserver * observer) = 0;
	virtual void UnregisterObserver(ISettingsObserver * observer) = 0;
};

class SettingsGroup
{
	NON_COPY_MOVABLE(SettingsGroup)

public:
	SettingsGroup(const ISettings & settings, const QString & group)
		: m_settings(settings)
	{
		m_settings.BeginGroup(group);
	}
	~SettingsGroup()
	{
		m_settings.EndGroup();
	}

private:
	const ISettings & m_settings;
};

}
