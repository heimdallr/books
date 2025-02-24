#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QPalette>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QStyleHints>

#include <Hypodermic/Hypodermic.h>

#include "fnd/FindPair.h"

#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IOpdsController.h"
#include "interface/logic/ITaskQueue.h"
#include "interface/ui/IMainWindow.h"

#include "logging/init.h"
#include "logic/model/LogModel.h"
#include "util/DyLib.h"
#include "util/ISettings.h"
#include "util/xml/Initializer.h"
#include "version/AppVersion.h"

#include "di_app.h"
#include "log.h"

#include "config/git_hash.h"
#include "config/version.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

QString ReadStyleSheet(const QString& fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		PLOGE << "Cannot open " << fileName;
		return {};
	}

	QTextStream ts(&file);
	return ts.readAll();
}

std::unique_ptr<Util::DyLib> SetStyle(QApplication& app, const ISettings& settings, bool& isCustomStyle)
{
	std::unique_ptr<Util::DyLib> result;
	QString styleSheet;

	const auto setStyle = [&](const QString& fileName)
	{
		styleSheet.append(ReadStyleSheet(fileName));
		isCustomStyle = true;
	};

	if (const auto qssVar = settings.Get(Constant::Settings::EXTERNAL_THEME_KEY); qssVar.isValid())
	{
		const auto qss = qssVar.toString();
		const auto qssExt = QFileInfo(qss).suffix().toLower();
		if (qssExt == "qss")
		{
			setStyle(qss);
		}
		else if (qssExt == "dll")
		{
			if (const auto qssFile = settings.Get(Constant::Settings::EXTERNAL_THEME_QSS_KEY); qssFile.isValid())
			{
				if ((result = std::make_unique<Util::DyLib>(qss.toStdString())))
					setStyle(qssFile.toString());
			}
		}
	}
	styleSheet.append(ReadStyleSheet(Constant::STYLE_FILE_NAME));
	app.setStyleSheet(styleSheet);

	return result;
}

std::unique_ptr<Util::DyLib> SetTheme(const ISettings& settings, const bool isCustomStyle)
{
	if (!isCustomStyle)
	{
		{
			auto style = settings.Get(Constant::Settings::THEME_KEY, Constant::Settings::APP_STYLE_DEFAULT);
			if (!QStyleFactory::keys().contains(style, Qt::CaseInsensitive))
				style = Constant::Settings::APP_STYLE_DEFAULT;

			QApplication::setStyle(style);
		}

		{
			constexpr std::pair<const char*, Qt::ColorScheme> schemes[] {
				{ "System", Qt::ColorScheme::Unknown },
				{  "Light",   Qt::ColorScheme::Light },
				{   "Dark",    Qt::ColorScheme::Dark },
			};

			const auto colorSchemeName = settings.Get(Constant::Settings::COLOR_SCHEME_KEY, Constant::Settings::APP_COLOR_SCHEME_DEFAULT);
			const auto scheme = FindSecond(schemes, colorSchemeName.toStdString().data(), Qt::ColorScheme::Unknown, PszComparer {});
			QGuiApplication::styleHints()->setColorScheme(scheme);

			if (scheme == Qt::ColorScheme::Unknown)
				QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, [] { QCoreApplication::exit(Constant::RESTART_APP); });
			else
				QGuiApplication::styleHints()->disconnect();
		}
	}

	const auto palette = QGuiApplication::palette();
	return std::make_unique<Util::DyLib>(palette.color(QPalette::WindowText).lightness() > palette.color(QPalette::Window).lightness() ? "theme_dark" : "theme_light");
}

} // namespace

int main(int argc, char* argv[])
{
	Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, PRODUCT_ID).toStdWString());
	LogModelAppender logModelAppender;

	PLOGI << "App started";
	PLOGI << "Version: " << GetApplicationVersion();
	PLOGI << "Commit hash: " << GIT_HASH;

	try
	{
		while (true)
		{
			QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

			QApplication app(argc, argv);
			QCoreApplication::setApplicationName(PRODUCT_ID);
			QCoreApplication::setApplicationVersion(PRODUCT_VERSION);
			Util::XMLPlatformInitializer xmlPlatformInitializer;

			PLOGD << "QApplication created";

			std::shared_ptr<Hypodermic::Container> container;
			{
				Hypodermic::ContainerBuilder builder;
				DiInit(builder, container);
			}
			PLOGD << "DI-container created";

			const auto settings = container->resolve<ISettings>();
			bool isCustomStyle = false;
			const auto styleLib = SetStyle(app, *settings, isCustomStyle);
			const auto themeLib = SetTheme(*settings, isCustomStyle);

			container->resolve<ITaskQueue>()->Execute();
			const auto mainWindow = container->resolve<IMainWindow>();
			container->resolve<IDatabaseUser>()->EnableApplicationCursorChange(true);
			mainWindow->Show();

			if (const auto code = QApplication::exec(); code != Constant::RESTART_APP)
			{
				PLOGI << "App finished with " << code;
				return code;
			}

			container->resolve<IOpdsController>()->Restart();

			PLOGI << "App restarted";
		}
	}
	catch (const std::exception& ex)
	{
		PLOGF << "App failed with " << ex.what();
	}
	catch (...)
	{
		PLOGF << "App failed with unknown error";
	}
}
