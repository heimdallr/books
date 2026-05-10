#include <thread>

#include <QApplication>
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTranslator>

#include "fnd/FindPair.h"

#include "interface/constants/SettingsConstant.h"
#include "interface/localization.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseController.h"
#include "interface/logic/IDatabaseMigrator.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IOpdsController.h"
#include "interface/logic/IScriptController.h"
#include "interface/logic/ISingleInstanceController.h"
#include "interface/logic/ITaskQueue.h"
#include "interface/logic/IUserDataController.h"
#include "interface/ui/IMainWindow.h"
#include "interface/ui/IMigrateWindow.h"
#include "interface/ui/IStyleApplierFactory.h"
#include "interface/ui/IUiFactory.h"

#include "Hypodermic/Hypodermic.h"
#include "logging/init.h"
#include "logic/data/Genre.h"
#include "logic/model/LogModel.h"
#include "platform/PlatformUtil.h"
#include "util/GenresLocalization.h"
#include "util/ISettings.h"
#include "util/xml/Initializer.h"
#include "version/AppVersion.h"

#include "Constant.h"
#include "di_app.h"
#include "log.h"

#include "config/git_hash.h"
#include "config/version.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

constexpr auto SEQ_NUMBER_WIDTH_KEY     = "Preferences/Export/seqNumberWidth";
constexpr auto KEYBOARD_LAYOUT_KEY      = "ui/recentKeyboardLayout";
constexpr auto SAVE_KEYBOARD_LAYOUT_KEY = "Preferences/saveKeyboardLayout";

constexpr auto CONTEXT = "Main";
constexpr auto WRONG_DB_VERSION =
	QT_TRANSLATE_NOOP("Main", "It looks like you're trying to use an older version of the app with a collection from the new version. This may cause instability. Are you sure you want to continue?");
constexpr auto UNSUPPORTED_DB_VERSION   = QT_TRANSLATE_NOOP("Main", "The database version is not supported. You need to save the user data and recreate the collection. Shall we do it?");
constexpr auto REMOVE_DATABASE_MANUALLY = QT_TRANSLATE_NOOP("Main", "In that case, before restarting the program please manually delete the collection database file:<br><br><b>%1</b><br>");
constexpr auto CANNOT_REMOVE_DATABASE   = QT_TRANSLATE_NOOP("Main", "The collection database file could not be deleted. Please delete it manually before restarting the program:<br><br><b>%1</b><br>");
TR_DEF

std::optional<Collection> RecreateDatabase(Hypodermic::Container& container)
{
	const auto uiFactory   = container.resolve<IUiFactory>();
	const auto showWarning = [&](const QString& message) {
		(void)uiFactory->ShowWarning(message);
		return std::nullopt;
	};

	if (uiFactory->ShowWarning(Tr(UNSUPPORTED_DB_VERSION), QMessageBox::No | QMessageBox::Yes, QMessageBox::Yes) == QMessageBox::No)
	{
		container.resolve<IDatabaseController>()->Reset();
		return showWarning(Tr(REMOVE_DATABASE_MANUALLY).arg(container.resolve<ICollectionProvider>()->GetActiveCollection().GetDatabase()));
	}

	auto activeCollection = [&] {
		const auto collectionProvider = container.resolve<ICollectionProvider>();
		assert(collectionProvider->ActiveCollectionExists());
		return collectionProvider->GetActiveCollection();
	}();
	const auto backupPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + activeCollection.id + ".flibk";

	QString errorMessage;

	QEventLoop eventLoop;
	container.resolve<IUserDataController>()->Backup(backupPath, [&](const bool ok, const QString& message) {
		PLOGV << message;
		if (!ok)
			errorMessage = message;
		eventLoop.exit();
	});
	eventLoop.exec();

	if (!errorMessage.isEmpty())
		return showWarning(errorMessage);

	container.resolve<IDatabaseController>()->Reset();

	QFile db(activeCollection.GetDatabase());
	db.remove();
	for (int i = 0; i < 20 && db.exists(); std::this_thread::sleep_for(std::chrono::milliseconds(50)), ++i)
		db.remove();

	if (db.exists())
		return showWarning(Tr(CANNOT_REMOVE_DATABASE).arg(activeCollection.GetDatabase()));

	return activeCollection;
}

} // namespace

int main(int argc, char* argv[])
{
	try
	{
		QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

		QApplication app(argc, argv);
		QCoreApplication::setApplicationName(PRODUCT_ID);
		QCoreApplication::setApplicationVersion(PRODUCT_VERSION);
		Util::XMLPlatformInitializer xmlPlatformInitializer;

		QCommandLineParser parser;
		parser.setApplicationDescription(QString("%1: another e-book cataloger").arg(PRODUCT_ID));
		parser.addHelpOption();
		parser.addVersionOption();

		const auto defaultLogPath = QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, PRODUCT_ID);
		const auto logOption      = Log::LoggingInitializer::AddLogFileOption(parser, defaultLogPath);
		parser.process(app);

		Log::LoggingInitializer logging(parser.isSet(logOption) ? parser.value(logOption) : defaultLogPath);
		LogModelAppender        logModelAppender;

		PLOGI << "App started";
		PLOGI << "Version: " << GetApplicationVersion();
		PLOGI << "Commit hash: " << GIT_HASH;
		// ReSharper disable CppCompileTimeConstantCanBeReplacedWithBooleanConstant
		if constexpr (PERSONAL_BUILD_NAME && PERSONAL_BUILD_NAME[0]) //-V560
		{
			PLOGI << "Personal build: " << PERSONAL_BUILD_NAME;
		}
		// ReSharper restore CppCompileTimeConstantCanBeReplacedWithBooleanConstant

		PLOGD << "QApplication created";

		Util::GenreFixerInitializer genreFixerInitializer;

		std::optional<Collection> collectionToRecreate;

		while (true)
		{
			std::shared_ptr<Hypodermic::Container> container;
			{
				Hypodermic::ContainerBuilder builder;
				DiInit(builder, container);
			}
			PLOGD << "DI-container created";

			const auto settings = container->resolve<ISettings>();

			if (settings->Get(SAVE_KEYBOARD_LAYOUT_KEY, false))
				if (const auto keyboardLayoutId = settings->Get(KEYBOARD_LAYOUT_KEY, QString {}); !keyboardLayoutId.isEmpty())
					Platform::SetKeyboardLayoutId(keyboardLayoutId);

			const auto translators = Loc::LoadLocales(*settings); //-V808

			auto singleInstanceController = container->resolve<ISingleInstanceController>();
			if (!singleInstanceController->IsFirstSingleInstanceApp())
				singleInstanceController.reset();

			Genre::SetSortMode(*settings);
			if (!settings->HasKey(QString(Constant::Settings::VIEW_NAVIGATION_KEY_TEMPLATE).arg(Loc::AllBooks)))
				settings->Set(QString(Constant::Settings::VIEW_NAVIGATION_KEY_TEMPLATE).arg(Loc::AllBooks), false);

			IScriptController::SetSeqNumberWidth(settings->Get(SEQ_NUMBER_WIDTH_KEY, 1));

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
					break;

				case IDatabaseMigrator::NeedMigrateResult::Unsupported:
					if ((collectionToRecreate = RecreateDatabase(*container)))
						continue;
					return EXIT_FAILURE;
			}

			container->resolve<ITaskQueue>()->Execute();
			const auto mainWindow = container->resolve<IMainWindow>();
			container->resolve<IDatabaseUser>()->EnableApplicationCursorChange(true);

			if (singleInstanceController)
				singleInstanceController->RegisterObserver(mainWindow.get());

			if (collectionToRecreate)
			{
				mainWindow->CreateCollection(std::move(*collectionToRecreate));
				collectionToRecreate = std::nullopt;
			}
			mainWindow->Show();

			if (const auto code = QApplication::exec(); code != Global::RESTART_APP)
			{
				if (settings->Get(SAVE_KEYBOARD_LAYOUT_KEY, false))
					settings->Set(KEYBOARD_LAYOUT_KEY, Platform::GetKeyboardLayoutId());
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
