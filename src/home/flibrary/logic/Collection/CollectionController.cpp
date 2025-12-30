#include "CollectionController.h"

#include <QTemporaryDir>
#include <QTimer>

#include "fnd/ScopedCall.h"
#include "fnd/observable.h"

#include "interface/Localization.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/ui/dialogs/IAddCollectionDialog.h"

#include "inpx/InpxConstant.h"
#include "util/IExecutor.h"
#include "util/files.h"

#include "CollectionImpl.h"
#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr int MAX_OVERWRITE_CONFIRM_COUNT = 10;

constexpr auto CONTEXT                          = "CollectionController";
constexpr auto CONFIRM_OVERWRITE_DATABASE       = QT_TRANSLATE_NOOP("CollectionController", "The existing database file will be overwritten. Continue?");
constexpr auto CONFIRM_REMOVE_COLLECTION        = QT_TRANSLATE_NOOP("CollectionController", "Are you sure you want to delete the collection?");
constexpr auto CONFIRM_REMOVE_DATABASE          = QT_TRANSLATE_NOOP("CollectionController", "Delete collection database as well?");
constexpr auto CANNOT_WRITE_TO_DATABASE         = QT_TRANSLATE_NOOP("CollectionController", "No write access to %1");
constexpr auto ERROR                            = QT_TRANSLATE_NOOP("CollectionController", "The collection was not %1 due to errors. See log.");
constexpr auto COLLECTION_UPDATED               = QT_TRANSLATE_NOOP("CollectionController", "Looks like the collection has been updated. Apply changes?");
constexpr auto COLLECTION_UPDATE_ACTION_CREATED = QT_TRANSLATE_NOOP("CollectionController", "created");
constexpr auto COLLECTION_UPDATE_ACTION_UPDATED = QT_TRANSLATE_NOOP("CollectionController", "updated");
constexpr auto COLLECTION_UPDATE_RESULT_GENRES  = QT_TRANSLATE_NOOP("CollectionController", "<tr><td>Genres:</td><td align='right'>%1</td></tr>");
constexpr auto COLLECTION_NEED_RECREATE =
	QT_TRANSLATE_NOOP("CollectionController", "<p><p>Warning! A change to previous data was detected, it is recommended to recreate the collection again. Don't forget to save user data</p></p>");
constexpr auto NO_UPDATES_FOUND         = QT_TRANSLATE_NOOP("CollectionController", "No updates found");
constexpr auto COLLECTION_UPDATE_RESULT = QT_TRANSLATE_NOOP("CollectionController", R"("%1" collection %2. Added:<p>
<table>
<tr><td>Archives:</td><td align='right'>%3</td></tr>
<tr><td>Authors:</td><td align='right'>%4</td></tr>
<tr><td>Series:</td><td align='right'>%5</td></tr>
<tr><td>Books:</td><td align='right'>%6</td></tr>
<tr><td>Keywords:</td><td align='right'>%7</td></tr>
%8
</table>%9)");

TR_DEF

} // namespace

class CollectionController::Impl final : public Observable<ICollectionsObserver>
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(std::shared_ptr<ICollectionProvider> collectionProvider, std::shared_ptr<ISettings> settings, std::shared_ptr<IUiFactory> uiFactory, const std::shared_ptr<ITaskQueue>& taskQueue)
		: m_collectionProvider { std::move(collectionProvider) }
		, m_settings { std::move(settings) }
		, m_uiFactory { std::move(uiFactory) }
		, m_taskQueue { taskQueue }
	{
		Register(m_collectionProvider.get());
		const auto& collections = m_collectionProvider->GetCollections();
		if (std::ranges::none_of(collections, [id = CollectionImpl::GetActive(*m_settings)](const auto& item) {
				return item->id == id;
			}))
			SetActiveCollection(collections.empty() ? QString {} : collections.front()->id);
	}

	~Impl() override
	{
		Unregister(m_collectionProvider.get());
	}

	Collections& GetCollections() noexcept
	{
		return m_collectionProvider->GetCollections();
	}

	const Collections& GetCollections() const noexcept
	{
		return m_collectionProvider->GetCollections();
	}

	void AddCollection(const std::filesystem::path& inpx = {})
	{
		const auto dialog = m_uiFactory->CreateAddCollectionDialog(inpx);
		const auto action = dialog->Exec();
		const auto mode   = Inpx::CreateCollectionMode::None | (dialog->AddUnIndexedBooks() ? Inpx::CreateCollectionMode::AddUnIndexedFiles : Inpx::CreateCollectionMode::None)
		                | (dialog->MarkUnIndexedBooksAsDeleted() ? Inpx::CreateCollectionMode::MarkUnIndexedFilesAsDeleted : Inpx::CreateCollectionMode::None)
		                | (dialog->ScanUnIndexedFolders() ? Inpx::CreateCollectionMode::ScanUnIndexedFolders : Inpx::CreateCollectionMode::None)
		                | (dialog->SkipLostBooks() ? Inpx::CreateCollectionMode::SkipLostBooks : Inpx::CreateCollectionMode::None);
		switch (action)
		{
			case IAddCollectionDialog::Result::CreateNew:
				CreateNew(dialog->GetName(), dialog->GetDatabaseFileName(), dialog->GetArchiveFolder(), dialog->GetDefaultArchiveType(), mode);
				break;

			case IAddCollectionDialog::Result::Add:
				Add(dialog->GetName(), dialog->GetDatabaseFileName(), dialog->GetArchiveFolder(), mode);
				break;

			default:
				break;
		}

		m_overwriteConfirmCount = 0;
	}

	void RescanCollectionFolder()
	{
		const auto& collection = GetActiveCollection();
		auto        parser     = std::make_shared<Inpx::Parser>();
		auto&       parserRef  = *parser;
		auto [tmpDir, ini]     = m_collectionProvider->GetIniMap(collection.GetDatabase(), collection.GetFolder(), true);
		auto callback          = [this, parser = std::move(parser), tmpDir = std::move(tmpDir), name = collection.name](const Inpx::UpdateResult& updateResult) mutable {
            const ScopedCall parserResetGuard([parser = std::move(parser)]() mutable {
                parser.reset();
            });
            Perform(&ICollectionsObserver::OnNewCollectionCreating, false);
            ShowUpdateResult(updateResult, name, COLLECTION_UPDATE_ACTION_UPDATED);
		};
		Perform(&ICollectionsObserver::OnNewCollectionCreating, true);
		parserRef.RescanCollection(ini, static_cast<Inpx::CreateCollectionMode>(collection.createCollectionMode), std::move(callback));
	}

	void RemoveCollection()
	{
		if (m_uiFactory->ShowWarning(Tr(CONFIRM_REMOVE_COLLECTION), QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Cancel)
			return;

		const auto& collection = GetActiveCollection();
		const auto  id         = collection.id;
		auto        db         = collection.GetDatabase();
		std::erase_if(m_collectionProvider->GetCollections(), [&](const auto& item) {
			return item->id == id;
		});
		CollectionImpl::Remove(*m_settings, id);

		if (m_uiFactory->ShowWarning(Tr(CONFIRM_REMOVE_DATABASE), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
		{
			ITaskQueue::Lock(m_taskQueue)->Enqueue([db = std::move(db)] {
				for (int i = 0; i < 20; ++i, std::this_thread::sleep_for(std::chrono::milliseconds(50)))
					if (QFile::remove(db) && !QFile::exists(db))
						return true;

				return false;
			});
		}

		if (m_collectionProvider->IsEmpty())
			return Perform(&ICollectionsObserver::OnActiveCollectionChanged);

		CollectionImpl::SetActive(*m_settings, m_collectionProvider->GetCollections().front()->id);
		Perform(&ICollectionsObserver::OnActiveCollectionChanged);
	}

	void SetActiveCollection(const QString& id)
	{
		CollectionImpl::SetActive(*m_settings, id);
		Perform(&ICollectionsObserver::OnActiveCollectionChanged);
	}

	void OnInpxUpdateChecked(const Collection& updatedCollection)
	{
		switch (m_uiFactory->ShowQuestion(Tr(COLLECTION_UPDATED), QMessageBox::Yes | QMessageBox::No | QMessageBox::Discard, QMessageBox::Yes)) // NOLINT(clang-diagnostic-switch-enum)
		{
			case QMessageBox::No:
				break;

			case QMessageBox::Yes:
				return UpdateCollection(updatedCollection);

			case QMessageBox::Discard:
				return CollectionImpl::Serialize(updatedCollection, *m_settings);

			default:
				assert(false && "unexpected button");
		}
	}

	void AllowDestructiveOperation(const bool value)
	{
		assert(ActiveCollectionExists());
		auto& collection                        = m_collectionProvider->GetActiveCollection();
		collection.destructiveOperationsAllowed = value;
		CollectionImpl::Serialize(collection, *m_settings);
	}

	IniMapPair GetIniMap(const QString& db, const QString& inpxFolder, const bool createFiles) const
	{
		return m_collectionProvider->GetIniMap(db, inpxFolder, createFiles);
	}

	Collection& GetActiveCollection() noexcept
	{
		return m_collectionProvider->GetActiveCollection();
	}

	const Collection& GetActiveCollection() const noexcept
	{
		return m_collectionProvider->GetActiveCollection();
	}

	bool ActiveCollectionExists() const noexcept
	{
		return m_collectionProvider->ActiveCollectionExists();
	}

	bool IsCollectionNameExists(const QString& name) const
	{
		return m_collectionProvider->IsCollectionNameExists(name);
	}

	QString GetCollectionDatabaseName(const QString& databaseFileName) const
	{
		return m_collectionProvider->GetCollectionDatabaseName(databaseFileName);
	}

	auto GetInpxFiles(const QString& folder) const
	{
		return m_collectionProvider->GetInpxFiles(folder);
	}

	bool IsCollectionFolderHasInpx(const QString& folder) const
	{
		return m_collectionProvider->IsCollectionFolderHasInpx(folder);
	}

	QString GetActiveCollectionId() const noexcept
	{
		return m_collectionProvider->GetActiveCollectionId();
	}

private:
	void CreateNew(QString name, QString dbOrigin, QString folderOrigin, const QString& defaultArchiveType, const Inpx::CreateCollectionMode mode)
	{
		const auto db     = Util::ToAbsolutePath(dbOrigin);
		const auto folder = Util::ToAbsolutePath(folderOrigin);
		if (QFile(db).exists())
		{
			if (m_uiFactory->ShowWarning(Tr(CONFIRM_OVERWRITE_DATABASE), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No)
				return ++m_overwriteConfirmCount < MAX_OVERWRITE_CONFIRM_COUNT ? AddCollection() : Perform(&ICollectionsObserver::OnActiveCollectionChanged);

			PLOGW << db << " will be rewritten";
		}

		if (!QFile(db).open(QIODevice::WriteOnly))
		{
			(void)m_uiFactory->ShowWarning(Tr(CANNOT_WRITE_TO_DATABASE).arg(db), QMessageBox::Ok);
			return;
		}

		auto  parser       = std::make_shared<Inpx::Parser>();
		auto& parserRef    = *parser;
		auto [tmpDir, ini] = m_collectionProvider->GetIniMap(db, folder, true);
		ini.try_emplace(DEFAULT_ARCHIVE_TYPE, defaultArchiveType.toStdWString());

		ini.try_emplace(SET_DATABASE_VERSION_STATEMENT, IDatabaseUser::GetDatabaseVersionStatement().toStdWString());
		auto callback = [this, parser = std::move(parser), name, db = std::move(dbOrigin), folder = std::move(folderOrigin), mode, tmpDir = std::move(tmpDir)](const Inpx::UpdateResult& updateResult) mutable {
			const ScopedCall parserResetGuard([parser = std::move(parser)]() mutable {
				parser.reset();
			});
			Perform(&ICollectionsObserver::OnNewCollectionCreating, false);
			ShowUpdateResult(updateResult, name, COLLECTION_UPDATE_ACTION_CREATED);
			if (!updateResult.error)
				Add(std::move(name), std::move(db), std::move(folder), mode);
		};
		Perform(&ICollectionsObserver::OnNewCollectionCreating, true);
		parserRef.CreateNewCollection(std::move(ini), mode, std::move(callback));
	}

	void Add(QString name, QString db, QString folder, const Inpx::CreateCollectionMode mode)
	{
		CollectionImpl collection(std::move(name), std::move(db), std::move(folder));
		collection.createCollectionMode = static_cast<int>(mode);
		CollectionImpl::Serialize(collection, *m_settings);
		auto& collections = m_collectionProvider->GetCollections();
		collections.push_back(std::make_unique<CollectionImpl>(std::move(collection)));
		SetActiveCollection(collections.back()->id);
	}

	void UpdateCollection(const Collection& updatedCollection)
	{
		const auto& collection = GetActiveCollection();
		auto        parser     = std::make_shared<Inpx::Parser>();
		auto&       parserRef  = *parser;
		auto [tmpDir, ini]     = m_collectionProvider->GetIniMap(collection.GetDatabase(), collection.GetFolder(), true);
		auto callback          = [this, parser = std::move(parser), tmpDir = std::move(tmpDir), name = collection.name](const Inpx::UpdateResult& updateResult) mutable {
            if (updateResult.oldDataUpdateFound)
                PLOGW << "Old indices changed. It is recommended to recreate the collection again.";
            const ScopedCall parserResetGuard([parser = std::move(parser)]() mutable {
                parser.reset();
            });
            Perform(&ICollectionsObserver::OnNewCollectionCreating, false);
            ShowUpdateResult(updateResult, name, COLLECTION_UPDATE_ACTION_UPDATED);
		};
		Perform(&ICollectionsObserver::OnNewCollectionCreating, true);
		parserRef.UpdateCollection(ini, static_cast<Inpx::CreateCollectionMode>(updatedCollection.createCollectionMode), std::move(callback));
	}

	void ShowUpdateResult(const Inpx::UpdateResult& updateResult, const QString& name, const char* action)
	{
		if (updateResult.error)
			return m_uiFactory->ShowError(Tr(ERROR).arg(Tr(action)));

		updateResult.folders == 0 ? m_uiFactory->ShowInfo(Tr(NO_UPDATES_FOUND))
								  : m_uiFactory->ShowInfo(Tr(COLLECTION_UPDATE_RESULT)
		                                                      .arg(name)
		                                                      .arg(Tr(action))
		                                                      .arg(updateResult.folders)
		                                                      .arg(updateResult.authors)
		                                                      .arg(updateResult.series)
		                                                      .arg(updateResult.books)
		                                                      .arg(updateResult.keywords)
		                                                      .arg(updateResult.genres ? Tr(COLLECTION_UPDATE_RESULT_GENRES).arg(updateResult.genres) : "")
		                                                      .arg(updateResult.oldDataUpdateFound ? Tr(COLLECTION_NEED_RECREATE) : ""));
	}

private:
	PropagateConstPtr<ICollectionProvider, std::shared_ptr> m_collectionProvider;
	PropagateConstPtr<ISettings, std::shared_ptr>           m_settings;
	PropagateConstPtr<IUiFactory, std::shared_ptr>          m_uiFactory;
	std::weak_ptr<ITaskQueue>                               m_taskQueue;
	int                                                     m_overwriteConfirmCount { 0 };
};

CollectionController::CollectionController(
	std::shared_ptr<ICollectionProvider> collectionProvider,
	std::shared_ptr<ISettings>           settings,
	std::shared_ptr<IUiFactory>          uiFactory,
	const std::shared_ptr<ITaskQueue>&   taskQueue
)
	: m_impl(std::move(collectionProvider), std::move(settings), std::move(uiFactory), taskQueue)
{
	PLOGV << "CollectionController created";
}

CollectionController::~CollectionController()
{
	PLOGV << "CollectionController destroyed";
}

void CollectionController::AddCollection(const std::filesystem::path& inpxDir)
{
	m_impl->AddCollection(inpxDir);
}

void CollectionController::RescanCollectionFolder()
{
	m_impl->RescanCollectionFolder();
}

void CollectionController::RemoveCollection()
{
	m_impl->RemoveCollection();
}

bool CollectionController::IsEmpty() const noexcept
{
	return GetCollections().empty();
}

bool CollectionController::IsCollectionNameExists(const QString& name) const
{
	return m_impl->IsCollectionNameExists(name);
}

QString CollectionController::GetCollectionDatabaseName(const QString& databaseFileName) const
{
	return m_impl->GetCollectionDatabaseName(databaseFileName);
}

std::set<QString> CollectionController::GetInpxFiles(const QString& folder) const
{
	return m_impl->GetInpxFiles(folder);
}

bool CollectionController::IsCollectionFolderHasInpx(const QString& folder) const
{
	return m_impl->IsCollectionFolderHasInpx(folder);
}

Collections& CollectionController::GetCollections() noexcept
{
	return m_impl->GetCollections();
}

const Collections& CollectionController::GetCollections() const noexcept
{
	return m_impl->GetCollections();
}

Collection& CollectionController::GetActiveCollection() noexcept
{
	return m_impl->GetActiveCollection();
}

const Collection& CollectionController::GetActiveCollection() const noexcept
{
	return m_impl->GetActiveCollection();
}

bool CollectionController::ActiveCollectionExists() const noexcept
{
	return m_impl->ActiveCollectionExists();
}

QString CollectionController::GetActiveCollectionId() const noexcept
{
	return m_impl->GetActiveCollectionId();
}

void CollectionController::SetActiveCollection(const QString& id)
{
	m_impl->SetActiveCollection(id);
}

void CollectionController::OnInpxUpdateChecked(const Collection& updatedCollection)
{
	m_impl->OnInpxUpdateChecked(updatedCollection);
}

void CollectionController::AllowDestructiveOperation(const bool value)
{
	m_impl->AllowDestructiveOperation(value);
}

ICollectionProvider::IniMapPair CollectionController::GetIniMap(const QString& db, const QString& inpxFolder, const bool createFiles) const
{
	return m_impl->GetIniMap(db, inpxFolder, createFiles);
}

void CollectionController::RegisterObserver(ICollectionsObserver* observer)
{
	m_impl->Register(observer);
}

void CollectionController::UnregisterObserver(ICollectionsObserver* observer)
{
	m_impl->Unregister(observer);
}
