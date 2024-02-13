#include <QCoreApplication>
#include <QString>

#include "interface/theme/ITheme.h"

#include "export/ThemeDefault.h"

namespace {

constexpr auto ID = "";
constexpr auto CONTEXT = "Theme";
constexpr auto TITLE = QT_TRANSLATE_NOOP("Theme", "De&fault");

class Theme final : virtual public HomeCompa::Flibrary::ITheme
{
	QString GetStyleSheet() const override
	{
		return {};
	}

	QString GetThemeId() const override
	{
		return ID;
	}

	QString GetThemeTitle() const override
	{
		return QCoreApplication::translate(CONTEXT, TITLE);
	}

};

}

extern "C" void THEMEDEFAULT_EXPORT Register(HomeCompa::Flibrary::IThemeRegistrar & registrar)
{
	const Theme theme;
	registrar.Register(theme);
}
