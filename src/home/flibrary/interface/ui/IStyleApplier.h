#pragma once

#include <utility>

#include <QString>
#include <QVariant>

namespace HomeCompa::Flibrary
{

class IStyleApplier // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	enum class Type
	{
		ColorScheme,
		PluginStyle,
		QssStyle,
		DllStyle,
	};

public:
	virtual ~IStyleApplier() = default;
	virtual Type GetType() const noexcept = 0;
	virtual void Apply(const QString& name, const QVariant& data) = 0;
	virtual std::pair<QString, QVariant> GetChecked() const = 0;
};

}
