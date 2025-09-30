#pragma once

#include <QVariant>

#include "fnd/NonCopyMovable.h"

namespace HomeCompa
{

class ISettingsObserver;

template <typename T>
concept IsEnum = std::is_enum_v<T>;

template <typename T>
concept IsBool = std::is_same_v<T, bool>;

template <typename T>
concept IsInt = std::is_integral_v<T> && !IsEnum<T> && !IsBool<T>;

template <typename T>
concept IsFloatingPoint = std::is_floating_point_v<T>;

template <typename T>
concept IsString = std::is_same_v<T, QString> || std::is_same_v<T, const char*>;

class ISettings // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ISettings() = default;

public:
	[[nodiscard]] virtual QVariant Get(const QString& key, const QVariant& defaultValue = {}) const = 0;
	virtual void                   Set(const QString& key, const QVariant& value)                   = 0;

	[[nodiscard]] virtual bool HasKey(const QString& key) const     = 0;
	[[nodiscard]] virtual bool HasGroup(const QString& group) const = 0;

	[[nodiscard]] virtual QStringList GetKeys() const   = 0;
	[[nodiscard]] virtual QStringList GetGroups() const = 0;

	virtual void Remove(const QString& key) = 0;

	virtual void RegisterObserver(ISettingsObserver* observer)   = 0;
	virtual void UnregisterObserver(ISettingsObserver* observer) = 0;

	template <IsString T>
	[[nodiscard]] QString Get(const QString& key, const T& defaultValue) const
	{
		return Get(key, QVariant::fromValue(QString(defaultValue))).toString();
	}

	template <IsEnum T>
	[[nodiscard]] T Get(const QString& key, const T& defaultValue) const
	{
		return static_cast<T>(Get(key, static_cast<int>(defaultValue)));
	}

	template <IsBool T>
	[[nodiscard]] T Get(const QString& key, const T& defaultValue) const
	{
		return Get(key, QVariant::fromValue(defaultValue)).toBool();
	}

	template <IsInt T>
	[[nodiscard]] T Get(const QString& key, const T& defaultValue) const
	{
		bool       ok    = false;
		const auto value = Get(key, QVariant::fromValue(defaultValue)).toLongLong(&ok);
		return ok ? static_cast<T>(value) : defaultValue;
	}

	template <IsFloatingPoint T>
	[[nodiscard]] T Get(const QString& key, const T& defaultValue) const
	{
		bool ok    = false;
		auto value = Get(key, QVariant::fromValue(defaultValue)).toDouble(&ok);
		return ok ? static_cast<T>(value) : defaultValue;
	}

private:
	virtual std::recursive_mutex& BeginGroup(const QString& group) const = 0;
	virtual void                  EndGroup() const                       = 0;
	friend class SettingsGroup;
};

class SettingsGroup
{
	NON_COPY_MOVABLE(SettingsGroup)

public:
	SettingsGroup(const ISettings& settings, const QString& group)
		: m_settings { settings }
		, m_lock { m_settings.BeginGroup(group) }
	{
	}

	~SettingsGroup()
	{
		m_settings.EndGroup();
	}

private:
	const ISettings&                      m_settings;
	std::lock_guard<std::recursive_mutex> m_lock;
};

} // namespace HomeCompa
