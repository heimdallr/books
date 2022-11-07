#pragma once

#include <QStringList>
#include <QVariant>

#include "fnd/memory.h"

#include "UtilLib.h"

namespace HomeCompa {

class SettingsGroup;
class UTIL_API Settings
{
	friend class SettingsGroup;

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
