#include "CollectionAutoUpdater.h"

#include <QCryptographicHash>

#include <QDir>
#include <QFileSystemWatcher>
#include <QTimer>

#include "fnd/ScopedCall.h"
#include "fnd/algorithm.h"
#include "fnd/observable.h"

#include "log.h"
#include "zip.h"

using namespace HomeCompa::Flibrary;

class CollectionAutoUpdater::Impl final : public Observable<IObserver>
{
public:
	Impl(std::shared_ptr<const ICollectionUpdateChecker> collectionUpdateChecker, std::shared_ptr<const ICollectionProvider> collectionProvider)
		: m_collectionProvider { std::move(collectionProvider) }
	{
		if (!m_collectionProvider->ActiveCollectionExists())
			return;

		auto& collectionUpdateCheckerRef = *collectionUpdateChecker;
		collectionUpdateCheckerRef.CheckForUpdate([this, collectionUpdateChecker = std::move(collectionUpdateChecker)](const bool result, const Collection&) mutable {
			result ? Update() : Init();
			collectionUpdateChecker.reset();
		});
	}

private:
	void Init()
	{
		PLOGD << "Init started";
		m_timer.setSingleShot(true);
		m_timer.setInterval(std::chrono::minutes(1));
		QObject::connect(&m_timer, &QTimer::timeout, [this] {
			Check();
		});

		QObject::connect(&m_watcher, &QFileSystemWatcher::fileChanged, [this](QString fileName) {
			PLOGI << fileName << " update detected";
			m_hash.try_emplace(std::move(fileName), QString {});
			m_timer.start();
		});

		const QDir folder(m_collectionProvider->GetActiveCollection().GetFolder());
		for (const auto& inpx : folder.entryList({ "*.inpx" }, QDir::Files))
			m_watcher.addPath(folder.filePath(inpx));

		PLOGD << "watch for: " << m_watcher.files().join(", ");
	}

	void Check()
	{
		PLOGD << "Check started";
		std::ranges::any_of(
			m_hash,
			[](auto& item) {
				QCryptographicHash hash(QCryptographicHash::Md5);
				QFile              file(item.first);
				if (!file.open(QIODevice::ReadOnly))
				{
					PLOGW << "Cannot read " << item.first;
					return true;
				}

				hash.addData(&file);
				if (Util::Set(item.second, QString::fromUtf8(hash.result().toHex())))
				{
					PLOGW << item.first << " hash changed: " << item.second;
					return true;
				}

				try
				{
					Zip zip(item.first);
					for (const auto& fileName : zip.GetFileNameList())
						PLOGV << fileName << ": " << zip.GetFileTime(fileName).toString() << ", " << zip.Read(fileName)->GetStream().readAll().size();
				}
				catch (...)
				{
					PLOGW << "Cannot unpack" << item.first;
					return true;
				}

				return false;
			}
		)
			? m_timer.start()
			: Update();
	}

	void Update() const
	{
		PLOGD << "Update started";
		const auto& collection = m_collectionProvider->GetActiveCollection();
		auto        parser     = std::make_shared<Inpx::Parser>();
		auto&       parserRef  = *parser;
		auto [tmpDir, ini]     = m_collectionProvider->GetIniMap(collection.GetDatabase(), collection.GetFolder(), collection.GetInpx(), true);
		auto callback          = [this, parser = std::move(parser), tmpDir = std::move(tmpDir)](const Inpx::UpdateResult& updateResult) mutable {
            if (updateResult.oldDataUpdateFound)
                PLOGW << "Old indices changed. It is recommended to recreate the collection again.";

            const ScopedCall parserResetGuard([parser = std::move(parser)]() mutable {
                parser.reset();
            });
            Perform(&IObserver::OnCollectionUpdated);
            PLOGD << "Update finished";
		};
		parserRef.UpdateCollection(ini, static_cast<Inpx::CreateCollectionMode>(collection.createCollectionMode), std::move(callback));
	}

private:
	const std::shared_ptr<const ICollectionProvider> m_collectionProvider;
	QTimer                                           m_timer;
	QFileSystemWatcher                               m_watcher;
	std::unordered_map<QString, QString>             m_hash;
};

CollectionAutoUpdater::CollectionAutoUpdater(std::shared_ptr<const ICollectionUpdateChecker> collectionUpdateChecker, std::shared_ptr<const ICollectionProvider> collectionProvider)
	: m_impl(std::move(collectionUpdateChecker), std::move(collectionProvider))
{
	PLOGV << "CollectionAutoUpdater created";
}

CollectionAutoUpdater::~CollectionAutoUpdater()
{
	PLOGV << "CollectionAutoUpdater destroyed";
}

void CollectionAutoUpdater::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void CollectionAutoUpdater::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
