#pragma once

#include <memory>

#include <QStringList>
#include <QVariant>

#include "fnd/NonCopyMovable.h"

#include "ISettings.h"

#include "export/util.h"

namespace HomeCompa
{

class UTIL_EXPORT Settings final : virtual public ISettings
{
	NON_COPY_MOVABLE(Settings)

public:
	explicit Settings(const QString& fileName);
	Settings(const QString& organization, const QString& application);
	~Settings() override;

public:
	[[nodiscard]] QVariant Get(const QString& key, const QVariant& defaultValue = {}) const override;
	void                   Set(const QString& key, const QVariant& value) override;

	[[nodiscard]] bool HasKey(const QString& key) const override;
	[[nodiscard]] bool HasGroup(const QString& group) const override;

	[[nodiscard]] QStringList GetKeys() const override;
	[[nodiscard]] QStringList GetGroups() const override;

	void Remove(const QString& key) override;

	std::recursive_mutex& BeginGroup(const QString& group) const override;
	void                  EndGroup() const override;

	void RegisterObserver(ISettingsObserver* observer) override;
	void UnregisterObserver(ISettingsObserver* observer) override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace HomeCompa
