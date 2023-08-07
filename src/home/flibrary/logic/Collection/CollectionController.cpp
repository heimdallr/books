#include "CollectionController.h"

#include <QTemporaryDir>
#include <QTimer>

#include <plog/Log.h>

#include "fnd/observable.h"

#include "interface/constants/Localization.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/ITaskQueue.h"
#include "interface/ui/dialogs/IAddCollectionDialog.h"
#include "interface/ui/IUiFactory.h"

#include "CollectionImpl.h"

#include "inpx/src/util/constant.h"
#include "inpx/src/util/inpx.h"
#include "util/IExecutor.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr int MAX_OVERWRITE_CONFIRM_COUNT = 10;

constexpr auto CONTEXT = "CollectionController";
constexpr auto CONFIRM_OVERWRITE_DATABASE = QT_TRANSLATE_NOOP("CollectionController", "The existing database file will be overwritten. Continue?");
constexpr auto CONFIRM_REMOVE_COLLECTION = QT_TRANSLATE_NOOP("CollectionController", "Are you sure you want to delete the collection?");
constexpr auto CONFIRM_REMOVE_DATABASE = QT_TRANSLATE_NOOP("CollectionController", "Delete collection database as well?");
constexpr auto CANNOT_WRITE_TO_DATABASE = QT_TRANSLATE_NOOP("CollectionController", "No write access to %1");

QString Tr(const char * str)
{
	return Loc::Tr(CONTEXT, str);
}

QString GetInpx(const QString & folder)
{
	const auto inpxList = QDir(folder).entryList({ "*.inpx" });
	auto result = inpxList.isEmpty() ? QString() : QString("%1/%2").arg(folder, inpxList.front());
	if (result.isEmpty())
		PLOGE << "Cannot find inpx in " << folder;

	return result;
}

using IniMap = std::map<std::wstring, std::filesystem::path>;
using IniMapPair = std::pair<std::unique_ptr<QTemporaryDir>, IniMap>;
IniMapPair GetIniMap(const QString & db, const QString & folder, bool createFiles)
{
	IniMapPair result { createFiles ? std::make_unique<QTemporaryDir>() : nullptr, IniMap{} };
	const auto getFile = [&tempDir = *result.first, createFiles] (const QString & name)
	{
		auto fileName = QDir::fromNativeSeparators(QCoreApplication::applicationDirPath() + QDir::separator() + name);
		if (!createFiles || QFile(fileName).exists())
			return fileName;

		fileName = tempDir.filePath(name);
		QFile::copy(":/data/" + name, fileName);
		return fileName;
	};

	const auto inpx = GetInpx(folder);
	if (inpx.isEmpty())
	{
		PLOGE << "Index file (*.inpx) not found";
		return result;
	}

	result.second = IniMap
	{
		{ DB_PATH, db.toStdWString() },
		{ GENRES, getFile("genres.ini").toStdWString() },
		{ DB_CREATE_SCRIPT, getFile("CreateCollection.sql").toStdWString() },
		{ DB_UPDATE_SCRIPT, getFile("UpdateCollection.sql").toStdWString() },
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
		, std::shared_ptr<ILogicFactory> logicFactory
		, std::shared_ptr<IUiFactory> uiFactory
		, std::shared_ptr<ITaskQueue> taskQueue
	)
		: m_settings(std::move(settings))
		, m_logicFactory(std::move(logicFactory))
		, m_uiFactory(std::move(uiFactory))
		, m_taskQueue(std::move(taskQueue))
	{
		if (std::ranges::none_of(m_collections, [id = CollectionImpl::GetActive(*m_settings)] (const auto & item) { return item->id == id; }))
			SetActiveCollection(m_collections.empty() ? QString {} : m_collections.front()->id);
	}

	const Collections & GetCollections() const noexcept
	{
		return m_collections;
	}

	void AddCollection()
	{
		switch (const auto dialog = m_uiFactory->CreateAddCollectionDialog(); dialog->Exec())
		{
			case IAddCollectionDialog::Result::CreateNew:
				CreateNew(dialog->GetName(), dialog->GetDatabaseFileName(), dialog->GetArchiveFolder());
				break;

			case IAddCollectionDialog::Result::Add:
				Add(dialog->GetName(), dialog->GetDatabaseFileName(), dialog->GetArchiveFolder());
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

		const auto collection = GetActiveCollection();
		assert(collection);
		const auto id = collection->id;
		auto db = collection->database;
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
			m_taskQueue->Enqueue([db = std::move(db)]
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

	std::optional<const Collection> GetActiveCollection() const noexcept
	{
		return FindCollectionById(CollectionImpl::GetActive(*m_settings));
	}

	std::optional<const Collection> FindCollectionById(const QString & id) const noexcept
	{
		const auto it = std::ranges::find_if(m_collections, [&] (const auto & item)
		{
			return item->id == id;
		});

		return it != std::cend(m_collections) ? **it : std::optional<const Collection>{};
	}

private:
	void CreateNew(QString name, QString db, QString folder)
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

		std::shared_ptr executor = m_logicFactory->GetExecutor({});
		(*executor)({"Create collection", [&, executor, name = std::move(name), db = std::move(db), folder = std::move(folder)]() mutable
		{
			auto result = std::function([executor](size_t) { });

			if (auto [_, ini] = GetIniMap(db, folder, true); Inpx::CreateNewCollection(std::move(ini)))
			{
				result = std::function([&, executor = std::move(executor), name = std::move(name), db = std::move(db), folder = std::move(folder)](size_t) mutable
				{
					Add(std::move(name), std::move(db), std::move(folder));
				});
			}
	
			return result;
		}});

	}

	void Add(QString name, QString db, QString folder)
	{
		CollectionImpl collection(std::move(name), std::move(db), std::move(folder));
		CollectionImpl::Serialize(collection, *m_settings);
		m_collections.push_back(std::make_unique<CollectionImpl>(std::move(collection)));
		SetActiveCollection(m_collections.back()->id);
	}

private:
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<ILogicFactory, std::shared_ptr> m_logicFactory;
	PropagateConstPtr<IUiFactory, std::shared_ptr> m_uiFactory;
	PropagateConstPtr<ITaskQueue, std::shared_ptr> m_taskQueue;
	Collections m_collections { CollectionImpl::Deserialize(*m_settings) };
	int m_overwriteConfirmCount { 0 };
};

CollectionController::CollectionController(std::shared_ptr<ISettings> settings
	, std::shared_ptr<ILogicFactory> logicFactory
	, std::shared_ptr<IUiFactory> uiFactory
	, std::shared_ptr<ITaskQueue> taskQueue
)
	: m_impl(std::move(settings)
		, std::move(logicFactory)
		, std::move(uiFactory)
		, std::move(taskQueue)
	)
{
	PLOGD << "CollectionController created";
}

CollectionController::~CollectionController()
{
	PLOGD << "CollectionController destroyed";
}

void CollectionController::AddCollection()
{
	m_impl->AddCollection();
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
	const auto collection = m_impl->FindCollectionById(CollectionImpl::GenerateId(databaseFileName));
	return collection ? collection->name : QString{};
}

bool CollectionController::IsCollectionFolderHasInpx(const QString & folder) const
{
	return !GetInpx(folder).isEmpty();
}

const Collections & CollectionController::GetCollections() const noexcept
{
	return m_impl->GetCollections();
}

std::optional<const Collection> CollectionController::GetActiveCollection() const noexcept
{
	return m_impl->GetActiveCollection();
}

void CollectionController::SetActiveCollection(const QString & id)
{
	m_impl->SetActiveCollection(id);
}

void CollectionController::RegisterObserver(IObserver * observer)
{
	m_impl->Register(observer);
}

void CollectionController::UnregisterObserver(IObserver * observer)
{
	m_impl->Unregister(observer);
}
