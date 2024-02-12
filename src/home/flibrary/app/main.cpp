#include <QApplication>
#include <QFile>
#include <QMainWindow>
#include <QStandardPaths>

#include <Hypodermic/Hypodermic.h>
#include <plog/Log.h>

#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/ITaskQueue.h"

#include "logging/init.h"

#include "logic/model/LogModel.h"

#include "di_app.h"

#include "gui/StyleUtils.h"
#include "util/ISettings.h"
#include "version/AppVersion.h"

#include "config/git_hash.h"
#include "config/version.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

void SetTheme(QApplication & app, const ISettings & settings)
{
	qApp->setStyleSheet({});
	StyleUtils::EnableSetHeaderViewStyle(true);

	const auto theme = settings.Get(Constant::Settings::THEME_KEY).toString();
	if (theme.isEmpty())
		return;

	{
		QFile f(QString(":theme/%1/%1style.qss").arg(theme));

		if (!f.open(QFile::ReadOnly | QFile::Text))
		{
			PLOGE << "Unable to set stylesheet, file not found";
			return;
		}

		QTextStream ts(&f);
		app.setStyleSheet(ts.readAll());
		StyleUtils::EnableSetHeaderViewStyle(false);
	}
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
		PLOGD << "QApplication created";

		QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

		while (true)
		{
			std::shared_ptr<Hypodermic::Container> container;
			{
				Hypodermic::ContainerBuilder builder;
				DiInit(builder, container);
			}
			PLOGD << "DI-container created";

			SetTheme(app, *container->resolve<ISettings>());

			container->resolve<ITaskQueue>()->Execute();
			const auto logicFactory = container->resolve<ILogicFactory>();
			const auto mainWindow = container->resolve<QMainWindow>();
			mainWindow->show();

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
