#include <QCoreApplication>
#include <QFile>
#include <QString>

#include <plog/Log.h>

#include "interface/theme/ITheme.h"

#include "export/ThemeDark.h"

namespace {

constexpr auto ID = "dark";
constexpr auto CONTEXT = "Theme";
constexpr auto TITLE = QT_TRANSLATE_NOOP("Theme", "&Dark");

class Theme final : virtual public HomeCompa::Flibrary::ITheme
{
	QString GetStyleSheet() const override
	{
		QFile f(QString(":theme/dark/darkstyle.qss"));

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

extern "C" void THEMEDARK_EXPORT Register(HomeCompa::Flibrary::IThemeRegistrar & registrar)
{
	const Theme theme;
	registrar.Register(theme);
}
