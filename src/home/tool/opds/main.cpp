#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QTimer>
#include <QTranslator>

#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include "fnd/ScopedCall.h"

#include "interface/IServer.h"
#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/localization.h"
#include "interface/logic/ICollectionAutoUpdater.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/IOpdsController.h"

#include "Hypodermic/Hypodermic.h"
#include "inpx/InpxConstant.h"
#include "logging/init.h"
#include "logic/Collection/CollectionImpl.h"
#include "logic/data/Genre.h"
#include "platform/NativeEventFilter.h"
#include "util/ISettings.h"
#include "util/SortString.h"
#include "util/xml/Initializer.h"

#include "Constant.h"
#include "di_app.h"
#include "log.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace HomeCompa::Flibrary;
using namespace Opds;

namespace
{

constexpr auto APP_ID = "opds";
constexpr auto NAME   = "name";

class NativeEventFilterObserver final : public Platform::NativeEventFilter::IObserver
{
private: // NativeEventFilter::IObserver
	void OnQueryEndSession(long long*) override
	{
		QCoreApplication::exit();
	}
};

class CollectionAutoUpdaterObserver final : ICollectionAutoUpdater::IObserver
{
	NON_COPY_MOVABLE(CollectionAutoUpdaterObserver)

public:
	explicit CollectionAutoUpdaterObserver(ICollectionAutoUpdater& updater)
		: m_updater { updater }
	{
		m_updater.RegisterObserver(this);
	}

	~CollectionAutoUpdaterObserver() override
	{
		m_updater.UnregisterObserver(this);
	}

private: // ICollectionAutoUpdater::IObserver
	void OnCollectionUpdated() override
	{
		QTimer::singleShot(0, [] {
			QCoreApplication::exit(Global::RESTART_APP);
		});
	}

private:
	ICollectionAutoUpdater& m_updater;
};

class CollectionControllerObserver final : public ICollectionsObserver
{
public:
	explicit CollectionControllerObserver(QEventLoop& eventLoop)
		: m_eventLoop { eventLoop }
	{
	}

private: // ICollectionsObserver
	void OnActiveCollectionChanged() override
	{
		m_eventLoop.exit();
	}

	void OnNewCollectionCreating(bool) override
	{
	}

private:
	QEventLoop& m_eventLoop;
};

bool CheckForStop(const QCommandLineParser& parser, Hypodermic::Container& container)
{
	if (!parser.isSet(Constant::OPDS_SERVER_COMMAND_STOP))
		return false;

	const auto opdsController = container.resolve<IOpdsController>();
	if (opdsController->IsRunning())
		return opdsController->Stop(), true;

	return false;
}

void SetCollection(const QCommandLineParser& parser, Hypodermic::Container& container)
{
	const auto collectionController = container.resolve<ICollectionController>();
	auto       name                 = parser.isSet(NAME) ? parser.value(NAME) : QString {};
	if (name.isEmpty())
	{
		if (collectionController->ActiveCollectionExists())
			return;

		throw std::runtime_error("Active collection not found");
	}

	const auto& collections = collectionController->GetCollections();
	if (const auto it = std::ranges::find(
			collections,
			name,
			[](const auto& collection) {
				return collection->name;
			}
		);
	    it != collections.end())
	{
		if (collectionController->ActiveCollectionExists() && (*it)->id == collectionController->GetActiveCollectionId())
			return;

		return collectionController->SetActiveCollection((*it)->id);
	}

	if (!parser.isSet(DB_PATH))
		throw std::invalid_argument("Database path required");
	if (!parser.isSet(ARCHIVE_FOLDER))
		throw std::invalid_argument("Archive folder required");

	QEventLoop                   eventLoop;
	CollectionControllerObserver observer(eventLoop);

	const auto collection = collectionController->CreateCollection(std::move(name), parser.value(DB_PATH), parser.value(ARCHIVE_FOLDER), parser.value(ADDITIONAL_FOLDER), parser.value(INPX_PATH));
	collectionController->RegisterObserver(&observer);
	collectionController->CreateCollection(*collection);
	eventLoop.exec();
	collectionController->UnregisterObserver(&observer);

	auto settings = container.resolve<ISettings>();
	settings->Set(Constant::Settings::PREFER_OPDS_AUTOUPDATE_COLLECTION, true);
}

int run(int argc, char* argv[])
{
	QGuiApplication app(argc, argv);
	QCoreApplication::setApplicationName(APP_ID);
	QCoreApplication::setApplicationVersion(PRODUCT_VERSION);
	Util::XMLPlatformInitializer xmlPlatformInitializer;

	QCommandLineParser parser;
	parser.setApplicationDescription(QString("%1 recodes images").arg(APP_ID));
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addOptions(
		{
			{ NAME, "Collection name", NAME },
			{ DB_PATH, "Database path", DB_PATH },
			{ ARCHIVE_FOLDER, "Archives folder", ARCHIVE_FOLDER },
			{ ADDITIONAL_FOLDER, "Additional data folder (optional)", ADDITIONAL_FOLDER },
			{ INPX_PATH, "Index inpx file (optional)", INPX_PATH },
			{ Constant::OPDS_SERVER_COMMAND_STOP, "Stop server" },
    }
	);
	parser.process(app);

	NativeEventFilterObserver   nativeEventFilterObserver;
	Platform::NativeEventFilter nativeEventFilter(app);
	const ScopedCall            nativeEventFilterRegisterGuard(
		[&] {
			nativeEventFilter.Register(&nativeEventFilterObserver);
		},
		[&] {
			nativeEventFilter.Unregister(&nativeEventFilterObserver);
		}
	);

	while (true)
	{
		std::shared_ptr<Hypodermic::Container> container;
		{
			Hypodermic::ContainerBuilder builder;
			DiInit(builder, container);
		}

		if (CheckForStop(parser, *container))
			return 0;

		SetCollection(parser, *container);

		auto settings = container->resolve<ISettings>();
		Genre::SetSortMode(*settings);

		std::shared_ptr<ICollectionAutoUpdater>        collectionAutoUpdater;
		std::unique_ptr<CollectionAutoUpdaterObserver> collectionAutoUpdaterObserver;
		if (settings->Get(Constant::Settings::PREFER_OPDS_AUTOUPDATE_COLLECTION, false))
		{
			collectionAutoUpdater         = container->resolve<ICollectionAutoUpdater>();
			collectionAutoUpdaterObserver = std::make_unique<CollectionAutoUpdaterObserver>(*collectionAutoUpdater);
		}

		Util::QStringWrapper::SetLocale(Loc::GetLocale(*settings));
		const auto translators = Loc::LoadLocales(*settings); //-V808
		const auto server      = container->resolve<IServer>();

		if (const auto code = QCoreApplication::exec(); code != Global::RESTART_APP)
		{
			PLOGI << "App finished with " << code;
			return code;
		}

		PLOGI << "App restarted";
	}
}

} // namespace

int main(const int argc, char* argv[])
{
	Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, APP_ID));
	PLOGI << QString("%1 started").arg(APP_ID);

	try
	{
		return run(argc, argv);
	}
	catch (const std::exception& ex)
	{
		PLOGE << ex.what();
	}
	catch (...)
	{
		PLOGE << "Unknown error";
	}

	return 1;
}
