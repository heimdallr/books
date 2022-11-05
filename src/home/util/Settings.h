#pragma once

#include "fnd/memory.h"

#include "UtilLib.h"

class QString;
class QVariant;

namespace HomeCompa {

class SettingsGroup;
class UTIL_API Settings
{
	friend class SettingsGroup;

public:
	Settings(const QString & organization, const QString & application);
	~Settings();

public:
	QVariant Get(const QString & key, const QVariant & defaultValue) const;
	void Set(const QString & key, const QVariant & value);

	bool HasKey(const QString & key) const;
	bool HasGroup(const QString & group) const;

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
