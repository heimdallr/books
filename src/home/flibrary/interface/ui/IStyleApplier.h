#pragma once

#include <utility>

#include <QString>

#include "util/DyLib.h"

#include "export/flint.h"

class QApplication;

namespace HomeCompa::Flibrary
{

class IStyleApplier // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
#ifndef NDEBUG
	static constexpr auto THEME_NAME_KEY = "ui/ThemeD/name";
	static constexpr auto THEME_TYPE_KEY = "ui/ThemeD/type";
	static constexpr auto THEME_FILE_KEY = "ui/ThemeD/file";
	static constexpr auto THEME_FILES_KEY = "ui/ThemeD/files";
#else
	static constexpr auto THEME_NAME_KEY = "ui/Theme/name";
	static constexpr auto THEME_TYPE_KEY = "ui/Theme/type";
	static constexpr auto THEME_FILE_KEY = "ui/Theme/file";
	static constexpr auto THEME_FILES_KEY = "ui/Theme/files";
#endif
	static constexpr auto THEME_NAME_DEFAULT = "windowsvista";
	static constexpr auto THEME_KEY_DEFAULT = "PluginStyle";
	static constexpr auto COLOR_SCHEME_KEY = "ui/colorScheme";
	static constexpr auto APP_COLOR_SCHEME_DEFAULT = "System";
	static constexpr auto STYLE_FILE_NAME = ":/theme/style.qss";

	static constexpr auto ACTION_PROPERTY_THEME_NAME = "value";
	static constexpr auto ACTION_PROPERTY_THEME_TYPE = "type";
	static constexpr auto ACTION_PROPERTY_THEME_FILE = "file";

public:
#define STYLE_APPLIER_TYPE_ITEMS_X_MACRO \
	STYLE_APPLIER_TYPE_ITEM(ColorScheme) \
	STYLE_APPLIER_TYPE_ITEM(PluginStyle) \
	STYLE_APPLIER_TYPE_ITEM(QssStyle)    \
	STYLE_APPLIER_TYPE_ITEM(DllStyle)

	enum class Type
	{
#define STYLE_APPLIER_TYPE_ITEM(NAME) NAME,
		STYLE_APPLIER_TYPE_ITEMS_X_MACRO
#undef STYLE_APPLIER_TYPE_ITEM
	};

public:
	FLINT_EXPORT static Type TypeFromString(const char* name);
	FLINT_EXPORT static QString TypeToString(Type type);

public:
	virtual ~IStyleApplier() = default;
	virtual Type GetType() const noexcept = 0;
	virtual void Apply(const QString& name, const QString& file) = 0;
	virtual std::pair<QString, QString> GetChecked() const = 0;
	virtual std::unique_ptr<Util::DyLib> Set(QApplication& app) const = 0;
};

} // namespace HomeCompa::Flibrary
