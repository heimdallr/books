#include <QCoreApplication>
#include <QStandardPaths>
#include <QTimer>
#include <QTranslator>

#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include "interface/IServer.h"
#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/ICollectionAutoUpdater.h"

#include "Hypodermic/Hypodermic.h"
#include "logging/init.h"
#include "logic/data/Genre.h"
#include "util/ISettings.h"
#include "util/SortString.h"
#include "util/localization.h"
#include "util/xml/Initializer.h"

#include "di_app.h"
#include "log.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace Opds;

namespace
{
constexpr auto APP_ID = "opds";

class CollectionAutoUpdaterObserver final : Flibrary::ICollectionAutoUpdater::IObserver
{
	NON_COPY_MOVABLE(CollectionAutoUpdaterObserver)

public:
	explicit CollectionAutoUpdaterObserver(Flibrary::ICollectionAutoUpdater& updater)
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
		QTimer::singleShot(0, [] { QCoreApplication::exit(Flibrary::Constant::RESTART_APP); });
	}

private:
	Flibrary::ICollectionAutoUpdater& m_updater;
};

int run(int argc, char* argv[])
{
	const QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName(APP_ID);
	QCoreApplication::setApplicationVersion(PRODUCT_VERSION);
	Util::XMLPlatformInitializer xmlPlatformInitializer;

	while (true)
	{
		std::shared_ptr<Hypodermic::Container> container;
		{
			Hypodermic::ContainerBuilder builder;
			DiInit(builder, container);
		}

		auto settings = container->resolve<ISettings>();
		Flibrary::Genre::SetSortMode(*settings);

		std::shared_ptr<Flibrary::ICollectionAutoUpdater> collectionAutoUpdater;
		std::unique_ptr<CollectionAutoUpdaterObserver> collectionAutoUpdaterObserver;
		if (settings->Get(Flibrary::Constant::Settings::OPDS_AUTOUPDATE_COLLECTION, false))
		{
			collectionAutoUpdater = container->resolve<Flibrary::ICollectionAutoUpdater>();
			collectionAutoUpdaterObserver = std::make_unique<CollectionAutoUpdaterObserver>(*collectionAutoUpdater);
		}

		Util::QStringWrapper::SetLocale(Loc::GetLocale(*settings));
		const auto translators = Loc::LoadLocales(*settings); //-V808
		const auto server = container->resolve<IServer>();

		if (const auto code = QCoreApplication::exec(); code != Flibrary::Constant::RESTART_APP)
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
	Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, APP_ID).toStdWString());
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
