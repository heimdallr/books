#include "CollectionController.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTimer>

#include <plog/Log.h>

#include "fnd/observable.h"
#include "fnd/ScopedCall.h"

#include "interface/constants/Localization.h"
#include "interface/constants/ProductConstant.h"
#include "interface/logic/ITaskQueue.h"
#include "interface/ui/dialogs/IAddCollectionDialog.h"
#include "interface/ui/IUiFactory.h"

#include "CollectionImpl.h"

#include "inpx/src/util/constant.h"
#include "inpx/src/util/inpx.h"
#include "util/hash.h"
#include "util/IExecutor.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr int MAX_OVERWRITE_CONFIRM_COUNT = 10;

constexpr auto CONTEXT = "CollectionController";
constexpr auto CONFIRM_OVERWRITE_DATABASE = QT_TRANSLATE_NOOP("CollectionController", "The existing database file will be overwritten. Continue?");
constexpr auto CONFIRM_REMOVE_COLLECTION = QT_TRANSLATE_NOOP("CollectionController", "Are you sure you want to delete the collection?");
constexpr auto CONFIRM_REMOVE_DATABASE = QT_TRANSLATE_NOOP("CollectionController", "Delete collection database as well?");
constexpr auto CANNOT_WRITE_TO_DATABASE = QT_TRANSLATE_NOOP("CollectionController", "No write access to %1");
constexpr auto COLLECTION_UPDATED = QT_TRANSLATE_NOOP("CollectionController", "Looks like the collection has been updated. Apply changes?");
constexpr auto COLLECTION_UPDATE_ACTION_CREATED = QT_TRANSLATE_NOOP("CollectionController", "created");
constexpr auto COLLECTION_UPDATE_ACTION_UPDATED = QT_TRANSLATE_NOOP("CollectionController", "updated");
constexpr auto COLLECTION_UPDATE_RESULT_GENRES = QT_TRANSLATE_NOOP("CollectionController", "<tr><td>Genres:</td><td>%1</td></tr>");
constexpr auto COLLECTION_UPDATE_RESULT = QT_TRANSLATE_NOOP("CollectionController"
	, R"("%1" collection %2. Added:
<table>
<tr><td>Archives:</td><td>%3</td></tr>
<tr><td>Authors: </td><td>%4</td></tr>
<tr><td>Series:  </td><td>%5</td></tr>
<tr><td>Books:   </td><td>%6</td></tr>
<tr><td>Keywords:</td><td>%7</td></tr>
%8
</table>)");

TR_DEF

QString GetInpxImpl(const QString & folder)
{
	const auto inpxList = QDir(folder).entryList({ "*.inpx" });
	return inpxList.isEmpty() ? QString{} : QString("%1/%2").arg(folder, inpxList.front());
}

using IniMapPair = std::pair<std::shared_ptr<QTemporaryDir>, Inpx::Parser::IniMap>;
IniMapPair GetIniMap(const QString & db, const QString & folder, bool createFiles)
{
	IniMapPair result { createFiles ? std::make_shared<QTemporaryDir>() : nullptr, Inpx::Parser::IniMap{} };
	const auto getFile = [&tempDir = *result.first, createFiles] (const QString & name)
	{
		auto fileName = QDir::fromNativeSeparators(QCoreApplication::applicationDirPath() + QDir::separator() + name);
		if (!createFiles || QFile(fileName).exists())
			return fileName;

		fileName = tempDir.filePath(name);
		QFile::copy(":/data/" + name, fileName);
		return fileName;
	};

	const auto inpx = GetInpxImpl(folder);

	result.second = Inpx::Parser::IniMap
	{
		{ DB_PATH, db.toStdWString() },
		{ GENRES, getFile(QString::fromStdWString(DEFAULT_GENRES)).toStdWString() },
		{ DB_CREATE_SCRIPT, getFile(QString::fromStdWString(DEFAULT_DB_CREATE_SCRIPT)).toStdWString() },
		{ DB_UPDATE_SCRIPT, getFile(QString::fromStdWString(DEFAULT_DB_UPDATE_SCRIPT)).toStdWString() },
		{ INPX, inpx.toStdWString() },
	};

	for (const auto & [key, value] : result.second)
		PLOGD << QString::fromStdWString(key) << ": " << QString::fromStdWString(value);

	return result;
}

}

class CollectionController::Impl final
	: public Observable<IObserver>
{
public:
	Impl(std::shared_ptr<ISettings> settings
		, std::shared_ptr<IUiFactory> uiFactory
		, const std::shared_ptr<ITaskQueue>& taskQueue
	)
		: m_settings(std::move(settings))
		, m_uiFactory(std::move(uiFactory))
		, m_taskQueue(taskQueue)
	{
		if (std::ranges::none_of(m_collections, [id = CollectionImpl::GetActive(*m_settings)] (const auto & item) { return item->id == id; }))
			SetActiveCollection(m_collections.empty() ? QString {} : m_collections.front()->id);
	}

	const Collections & GetCollections() const noexcept
	{
		return m_collections;
	}

	void AddCollection(const std::filesystem::path & inpx = {})
	{
		const auto dialog = m_uiFactory->CreateAddCollectionDialog(inpx);
		const auto action = dialog->Exec();
		const auto mode = Inpx::CreateCollectionMode::None
			| (dialog->AddUnIndexedBooks() ? Inpx::CreateCollectionMode::AddUnIndexedFiles : Inpx::CreateCollectionMode::None)
			| (dialog->ScanUnIndexedFolders() ? Inpx::CreateCollectionMode::ScanUnIndexedFolders : Inpx::CreateCollectionMode::None)
			;
		switch (action)
		{
			case IAddCollectionDialog::Result::CreateNew:
				CreateNew(dialog->GetName(), dialog->GetDatabaseFileName(), dialog->GetArchiveFolder(), mode);
				break;

			case IAddCollectionDialog::Result::Add:
				Add(dialog->GetName(), dialog->GetDatabaseFileName(), dialog->GetArchiveFolder(), mode);
				break;

			default:
				break;
		}

		m_overwriteConfirmCount = 0;
	}

	void RemoveCollection()
	{
		if (m_uiFactory->ShowWarning(Tr(CONFIRM_REMOVE_COLLECTION), QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Cancel)
			return;

		const auto& collection = GetActiveCollection();
		const auto id = collection.id;
		auto db = collection.database;
		if (const auto [begin, end] = std::ranges::remove_if(m_collections, [&] (const auto & item)
		{
			return item->id == id;
		}); begin != end)
		{
			m_collections.erase(begin, end);
		}
		CollectionImpl::Remove(*m_settings, id);

		if (m_uiFactory->ShowWarning(Tr(CONFIRM_REMOVE_DATABASE), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
		{
			ITaskQueue::Lock(m_taskQueue)->Enqueue([db = std::move(db)]
			{
				for (int i = 0; i < 20; ++i, std::this_thread::sleep_for(std::chrono::milliseconds(50)))
					if (QFile::remove(db) && !QFile::exists(db))
						return true;

				return false;
			});
		}

		if (m_collections.empty())
			return Perform(&IObserver::OnActiveCollectionChanged);

		CollectionImpl::SetActive(*m_settings, m_collections.front()->id);
		Perform(&IObserver::OnActiveCollectionChanged);
	}

	void SetActiveCollection(const QString & id)
	{
		CollectionImpl::SetActive(*m_settings, id);
		Perform(&IObserver::OnActiveCollectionChanged);
	}

	void OnInpxUpdateFound(const Collection & updatedCollection)
	{
		switch (m_uiFactory->ShowQuestion(Tr(COLLECTION_UPDATED), QMessageBox::Yes | QMessageBox::No | QMessageBox::Discard, QMessageBox::Yes))  // NOLINT(clang-diagnostic-switch-enum)
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

	const Collection& GetActiveCollection() const noexcept
	{
		const auto collection = FindCollectionById(GetActiveCollectionId());
		assert(collection);
		return *collection;
	}

	bool ActiveCollectionExists() const noexcept
	{
		const auto collection = FindCollectionById(GetActiveCollectionId());
		return !!collection;
	}

	const Collection* FindCollectionById(const QString & id) const noexcept
	{
		const auto it = std::ranges::find(m_collections, id, [&] (const auto & item) { return item->id; });
		if (it == std::cend(m_collections))
			return nullptr;

		const auto & collection = **it;
		return &collection;
	}

	QString GetActiveCollectionId() const noexcept
	{
		return CollectionImpl::GetActive(*m_settings);
	}

private:
	void CreateNew(QString name, QString db, QString folder, const Inpx::CreateCollectionMode mode)
	{
		if (QFile(db).exists())
		{
			if (m_uiFactory->ShowWarning(Tr(CONFIRM_OVERWRITE_DATABASE), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No)
				return ++m_overwriteConfirmCount < MAX_OVERWRITE_CONFIRM_COUNT
					? AddCollection()
					: Perform(&IObserver::OnActiveCollectionChanged);

			PLOGW << db << " will be rewritten";
		}

		if (!QFile(db).open(QIODevice::WriteOnly))
		{
			(void)m_uiFactory->ShowWarning(Tr(CANNOT_WRITE_TO_DATABASE).arg(db), QMessageBox::Ok);
			return;
		}

		auto parser = std::make_shared<Inpx::Parser>();
		auto & parserRef = *parser;
		auto [tmpDir, ini] = GetIniMap(db, folder, true);
		auto callback = [this, parser = std::move(parser), name, db, folder, mode, tmpDir = std::move(tmpDir)] (const Inpx::UpdateResult & updateResult) mutable
		{
			const ScopedCall parserResetGuard([parser = std::move(parser)] () mutable { parser.reset(); });
			Perform(&IObserver::OnNewCollectionCreating, false);
			ShowUpdateResult(updateResult, name, COLLECTION_UPDATE_ACTION_CREATED);
			Add(std::move(name), std::move(db), std::move(folder), mode);
		};
		Perform(&IObserver::OnNewCollectionCreating, true);
		parserRef.CreateNewCollection(std::move(ini), mode, std::move(callback));
	}

	void Add(QString name, QString db, QString folder, const Inpx::CreateCollectionMode mode)
	{
		CollectionImpl collection(std::move(name), std::move(db), std::move(folder));
		collection.createCollectionMode = static_cast<int>(mode);
		CollectionImpl::Serialize(collection, *m_settings);
		m_collections.push_back(std::make_unique<CollectionImpl>(std::move(collection)));
		SetActiveCollection(m_collections.back()->id);
	}

	void UpdateCollection(const Collection & updatedCollection)
	{
		const auto& collection = GetActiveCollection();
		auto parser = std::make_shared<Inpx::Parser>();
		auto & parserRef = *parser;
		auto [tmpDir, ini] = GetIniMap(collection.database, collection.folder, true);
		auto callback = [this, parser = std::move(parser), tmpDir = std::move(tmpDir), name = collection.name] (const Inpx::UpdateResult & updateResult) mutable
		{
			const ScopedCall parserResetGuard([parser = std::move(parser)] () mutable { parser.reset(); });
			Perform(&IObserver::OnNewCollectionCreating, false);
			ShowUpdateResult(updateResult, name, COLLECTION_UPDATE_ACTION_UPDATED);
		};
		Perform(&IObserver::OnNewCollectionCreating, true);
		parserRef.UpdateCollection(GetIniMap(collection.database, collection.folder, true).second, static_cast<Inpx::CreateCollectionMode>(updatedCollection.createCollectionMode), std::move(callback));
	}

	void ShowUpdateResult(const Inpx::UpdateResult & updateResult, const QString & name, const char * action)
	{
		if (updateResult.folders == 0)
			return;

		m_uiFactory->ShowInfo(Tr(COLLECTION_UPDATE_RESULT)
			.arg(name)
			.arg(Tr(action))
			.arg(updateResult.folders)
			.arg(updateResult.authors)
			.arg(updateResult.series)
			.arg(updateResult.books)
			.arg(updateResult.keywords)
			.arg(updateResult.genres ? Tr(COLLECTION_UPDATE_RESULT_GENRES).arg(updateResult.genres) : "")
		);
		QCoreApplication::exit(Constant::RESTART_APP);
	}

private:
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<IUiFactory, std::shared_ptr> m_uiFactory;
	std::weak_ptr<ITaskQueue> m_taskQueue;
	Collections m_collections { CollectionImpl::Deserialize(*m_settings) };
	int m_overwriteConfirmCount { 0 };
};

CollectionController::CollectionController(std::shared_ptr<ISettings> settings
	, std::shared_ptr<IUiFactory> uiFactory
	, const std::shared_ptr<ITaskQueue>& taskQueue
)
	: m_impl(std::move(settings)
		, std::move(uiFactory)
		, taskQueue
	)
{
	PLOGD << "CollectionController created";
}

CollectionController::~CollectionController()
{
	PLOGD << "CollectionController destroyed";
}

void CollectionController::AddCollection(const std::filesystem::path & inpx)
{
	m_impl->AddCollection(inpx);
}

void CollectionController::RemoveCollection()
{
	m_impl->RemoveCollection();
}

bool CollectionController::IsEmpty() const noexcept
{
	return GetCollections().empty();
}

bool CollectionController::IsCollectionNameExists(const QString & name) const
{
	return std::ranges::any_of(GetCollections(), [&] (const auto & item)
	{
		return item->name == name;
	});
}

QString CollectionController::GetCollectionDatabaseName(const QString & databaseFileName) const
{
	const auto collection = m_impl->FindCollectionById(Util::md5(databaseFileName.toUtf8()));
	return collection ? collection->name : QString{};
}

QString CollectionController::GetInpx(const QString & folder) const
{
	return GetInpxImpl(folder);
}

bool CollectionController::IsCollectionFolderHasInpx(const QString & folder) const
{
	return !GetInpx(folder).isEmpty();
}

const Collections & CollectionController::GetCollections() const noexcept
{
	return m_impl->GetCollections();
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

void CollectionController::SetActiveCollection(const QString & id)
{
	m_impl->SetActiveCollection(id);
}

void CollectionController::OnInpxUpdateFound(const Collection & updatedCollection)
{
	m_impl->OnInpxUpdateFound(updatedCollection);
}

void CollectionController::RegisterObserver(IObserver * observer)
{
	m_impl->Register(observer);
}

void CollectionController::UnregisterObserver(IObserver * observer)
{
	m_impl->Unregister(observer);
}
