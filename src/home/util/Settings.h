#pragma once

#include <QStringList>
#include <QVariant>

#include "fnd/memory.h"
#include "interface/ISettings.h"

#include "UtilLib.h"

namespace HomeCompa {

class UTIL_API Settings : virtual public ISettings
{
public:
	Settings(const QString & organization, const QString & application);
	~Settings();

public:
	QVariant Get(const QString & key, const QVariant & defaultValue = {}) const override;
	void Set(const QString & key, const QVariant & value) override;

	bool HasKey(const QString & key) const override;
	bool HasGroup(const QString & group) const override;

	QStringList GetKeys() const override;
	QStringList GetGroups() const override;

	void Remove(const QString & key) override;

	void BeginGroup(const QString & group) override;
	void EndGroup() override;

	void RegisterObserver(ISettingsObserver * observer) override;
	void UnregisterObserver(ISettingsObserver * observer) override;

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
