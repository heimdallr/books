#pragma once

#include <QStringList>
#include <QVariant>

#include "fnd/memory.h"

#include "UtilLib.h"

namespace HomeCompa {

class SettingsObserver;

class UTIL_API Settings
{
public:
	Settings(const QString & organization, const QString & application);
	~Settings();

public:
	QVariant Get(const QString & key, const QVariant & defaultValue = {}) const;
	void Set(const QString & key, const QVariant & value);

	bool HasKey(const QString & key) const;
	bool HasGroup(const QString & group) const;

	QStringList GetKeys() const;
	QStringList GetGroups() const;

	void Remove(const QString & key);

	void BeginGroup(const QString & group);
	void EndGroup();

	void RegisterObserver(SettingsObserver * observer);
	void UnregisterObserver(SettingsObserver * observer);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

class UTIL_API SettingsGroup
{
public:
	SettingsGroup(Settings & settings, const QString & group);
	~SettingsGroup();

private:
	Settings & m_settings;
};

}
