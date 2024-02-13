#pragma once

class QString;

namespace HomeCompa::Flibrary {

class ITheme  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ITheme() = default;
	virtual QString GetStyleSheet() const = 0;
	virtual QString GetThemeId() const = 0;
	virtual QString GetThemeTitle() const = 0;
};

class IThemeRegistrar  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IThemeRegistrar() = default;
	virtual void Register(const ITheme & theme) = 0;
};

}
