#include <QApplication>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTranslator>

#include "fnd/FindPair.h"

#include "interface/constants/Localization.h"
#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IDatabaseMigrator.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IOpdsController.h"
#include "interface/logic/ISingleInstanceController.h"
#include "interface/logic/ITaskQueue.h"
#include "interface/ui/IMainWindow.h"
#include "interface/ui/IMigrateWindow.h"
#include "interface/ui/IStyleApplierFactory.h"
#include "interface/ui/IUiFactory.h"

#include "Hypodermic/Hypodermic.h"
#include "logging/init.h"
#include "logic/data/Genre.h"
#include "logic/model/LogModel.h"
#include "util/ISettings.h"
#include "util/localization.h"
#include "util/xml/Initializer.h"
#include "version/AppVersion.h"

#include "di_app.h"
#include "log.h"

#include "config/git_hash.h"
#include "config/version.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{
constexpr auto CONTEXT = "Main";
constexpr auto WRONG_DB_VERSION =
	QT_TRANSLATE_NOOP("Main", "It looks like you're trying to use an older version of the app with a collection from the new version. This may cause instability. Are you sure you want to continue?");
TR_DEF
}

int main(int argc, char* argv[])
{
	Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, PRODUCT_ID).toStdWString());
	LogModelAppender        logModelAppender;

	PLOGI << "App started";
	PLOGI << "Version: " << GetApplicationVersion();
	PLOGI << "Commit hash: " << GIT_HASH;
	// ReSharper disable CppCompileTimeConstantCanBeReplacedWithBooleanConstant
	if constexpr (PERSONAL_BUILD_NAME && PERSONAL_BUILD_NAME[0]) //-V560
		PLOGI << "Personal build: " << PERSONAL_BUILD_NAME;
	// ReSharper restore CppCompileTimeConstantCanBeReplacedWithBooleanConstant

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

			const auto settings    = container->resolve<ISettings>();
			const auto translators = Loc::LoadLocales(*settings); //-V808

			auto singleInstanceController = container->resolve<ISingleInstanceController>();
			if (!singleInstanceController->IsFirstSingleInstanceApp())
				singleInstanceController.reset();

			Genre::SetSortMode(*settings);
			if (!settings->HasKey(QString(Constant::Settings::VIEW_NAVIGATION_KEY_TEMPLATE).arg(Loc::AllBooks)))
				settings->Set(QString(Constant::Settings::VIEW_NAVIGATION_KEY_TEMPLATE).arg(Loc::AllBooks), false);

			auto       styleApplierFactory = container->resolve<IStyleApplierFactory>();
			const auto themeLib            = styleApplierFactory->CreateThemeApplier()->Set(app);
			const auto colorSchemeLib      = styleApplierFactory->CreateStyleApplier(IStyleApplier::Type::ColorScheme)->Set(app);
			styleApplierFactory.reset();

			switch (const auto migrator = container->resolve<IDatabaseMigrator>(); migrator->NeedMigrate())
			{
				default:
					assert(false && "unexpected result");
					[[fallthrough]];

				case IDatabaseMigrator::NeedMigrateResult::Actual:
					break;

				case IDatabaseMigrator::NeedMigrateResult::NeedMigrate:
				{
					const auto migrateWindow = container->resolve<IMigrateWindow>();
					migrateWindow->Show();
					QApplication::exec();
					continue;
				}

				case IDatabaseMigrator::NeedMigrateResult::Unexpected:
					if (container->resolve<IUiFactory>()->ShowWarning(Tr(WRONG_DB_VERSION), QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
						return EXIT_FAILURE;
			}

			container->resolve<ITaskQueue>()->Execute();
			const auto mainWindow = container->resolve<IMainWindow>();
			container->resolve<IDatabaseUser>()->EnableApplicationCursorChange(true);

			if (singleInstanceController)
				singleInstanceController->RegisterObserver(mainWindow.get());

			mainWindow->Show();

			if (const auto code = QApplication::exec(); code != Constant::RESTART_APP)
			{
				PLOGI << "App finished with " << code;
				return code;
			}

			if (singleInstanceController)
				singleInstanceController->UnregisterObserver(mainWindow.get());

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
