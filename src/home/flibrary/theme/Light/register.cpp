#include <QCoreApplication>
#include <QFile>
#include <QString>

#include <plog/Log.h>

#include "interface/theme/ITheme.h"

#include "export/ThemeLight.h"

namespace {

constexpr auto ID = "light";
constexpr auto CONTEXT = "Theme";
constexpr auto TITLE = QT_TRANSLATE_NOOP("Theme", "&Light");

class Theme final : virtual public HomeCompa::Flibrary::ITheme
{
	QString GetStyleSheet() const override
	{
		QFile f(QString(":theme/light/lightstyle.qss"));

		if (!f.open(QFile::ReadOnly | QFile::Text))
		{
			PLOGE << "Unable to set stylesheet, file not found";
			return {};
		}

		QTextStream ts(&f);
		return ts.readAll();
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

extern "C" void THEMELIGHT_EXPORT Register(HomeCompa::Flibrary::IThemeRegistrar & registrar)
{
	const Theme theme;
	registrar.Register(theme);
}
