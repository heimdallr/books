#pragma once

#include <QStringList>
#include <QVariant>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "ISettings.h"

#include "UtilLib.h"

namespace HomeCompa {

class UTIL_API Settings final : virtual public ISettings
{
	NON_COPY_MOVABLE(Settings)

public:
	Settings(const QString & organization, const QString & application);
	~Settings() override;

public:
	[[nodiscard]] QVariant Get(const QString & key, const QVariant & defaultValue = {}) const override;
	void Set(const QString & key, const QVariant & value) override;

	[[nodiscard]] bool HasKey(const QString & key) const override;
	[[nodiscard]] bool HasGroup(const QString & group) const override;

	[[nodiscard]] QStringList GetKeys() const override;
	[[nodiscard]] QStringList GetGroups() const override;

	void Remove(const QString & key) override;

	void BeginGroup(const QString & group) override;
	void EndGroup() override;

	void RegisterObserver(ISettingsObserver * observer) override;
	void UnregisterObserver(ISettingsObserver * observer) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
