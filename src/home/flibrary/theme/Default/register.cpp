#include <QFile>
#include <QString>
#include <QTextStream>

#include <plog/Log.h>

#include "interface/theme/ITheme.h"

#include "export/ThemeDefault.h"

namespace {

constexpr auto ID = "";
constexpr auto TITLE = QT_TRANSLATE_NOOP("Theme", "De&fault");

class Theme final : virtual public HomeCompa::Flibrary::ITheme
{
	QString GetStyleSheet() const override
	{
		QFile f(QString(":theme/default/defaultstyle.qss"));

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
		return TITLE;
	}
};

}

extern "C" void THEMEDEFAULT_EXPORT Register(HomeCompa::Flibrary::IThemeRegistrar & registrar)
{
	const Theme theme;
	registrar.Register(theme);
}
