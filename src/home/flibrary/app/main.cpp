#include <QApplication>
#include <QFile>
#include <QStandardPaths>
#include <QStyleHints>

#include <Hypodermic/Hypodermic.h>
#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "interface/constants/Enums.h"
#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/ITaskQueue.h"
#include "interface/ui/IMainWindow.h"

#include "logging/init.h"

#include "logic/model/LogModel.h"

#include "di_app.h"

#include "util/ISettings.h"
#include "util/xml/Initializer.h"
#include "version/AppVersion.h"

#include "config/git_hash.h"
#include "config/version.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto STYLE_FILE_NAME = ":/theme/style.qss";

void SetStyle(QApplication & app)
{
	QFile file(STYLE_FILE_NAME);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		PLOGE << "Cannot open " << STYLE_FILE_NAME;
		return;
	}

	QTextStream ts(&file);
	app.setStyleSheet(ts.readAll());
}

void SetTheme(const ISettings & settings)
{
	static constexpr std::pair<const char *, Qt::ColorScheme> defaultTheme = { "windowsvista", Qt::ColorScheme::Light };
	static constexpr std::pair<const char *, std::pair<const char *, Qt::ColorScheme>> themes[]
	{
		{ AppTheme::WindowsVista  , { "windowsvista", Qt::ColorScheme::Light } },
		{ AppTheme::WindowsClassic, { "windows"     , Qt::ColorScheme::Light } },
		{ AppTheme::FusionSystem  , { "fusion"      , Qt::ColorScheme::Unknown } },
		{ AppTheme::FusionLight   , { "fusion"      , Qt::ColorScheme::Light } },
		{ AppTheme::FusionDark    , { "fusion"      , Qt::ColorScheme::Dark } },
	};

	const auto theme = settings.Get(Constant::Settings::THEME_KEY, AppTheme::WindowsVista);
	const auto& [style, scheme] = FindSecond(themes, theme.toStdString().data(), defaultTheme, PszComparer {});
	QApplication::setStyle(style);
	QGuiApplication::styleHints()->setColorScheme(scheme);
}

}

int main(int argc, char * argv[])
{
	Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, PRODUCT_ID).toStdWString());
	LogModelAppender logModelAppender;

	PLOGI << "App started";
	PLOGI << "Version: " << GetApplicationVersion();
	PLOGI << "Commit hash: " << GIT_HASH;

	try
	{
		QApplication app(argc, argv);
		QCoreApplication::setApplicationName(PRODUCT_ID);
		QCoreApplication::setApplicationVersion(PRODUCT_VERSION);
		Util::XMLPlatformInitializer xmlPlatformInitializer;

		PLOGD << "QApplication created";
		SetStyle(app);

		QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

		while (true)
		{
			std::shared_ptr<Hypodermic::Container> container;
			{
				Hypodermic::ContainerBuilder builder;
				DiInit(builder, container);
			}
			PLOGD << "DI-container created";

			SetTheme(*container->resolve<ISettings>());
			container->resolve<ITaskQueue>()->Execute();
			const auto mainWindow = container->resolve<IMainWindow>();
			mainWindow->Show();

			if (const auto code = QApplication::exec(); code != Constant::RESTART_APP)
			{
				PLOGI << "App finished with " << code;
				return code;
			}

			PLOGI << "App restarted";
		}
	}
	catch (const std::exception & ex)
	{
		PLOGF << "App failed with " << ex.what();
	}
	catch (...)
	{
		PLOGF << "App failed with unknown error";
	}
}
