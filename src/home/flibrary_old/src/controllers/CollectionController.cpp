#include <set>

#include <QAbstractItemModel>
#include <QApplication>
#include <QCryptographicHash>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QTimer>

#include <plog/Log.h>

#include "fnd/algorithm.h"
#include "models/SimpleModel.h"

#include "inpx/src/util/constant.h"
#include "inpx/src/util/inpx.h"
#include "inpx/src/util/StrUtil.h"

#include "util/IExecutor.h"
#include "util/executor/factory.h"

#include "constants/ObjectConnectorConstant.h"

#include "CollectionImpl.h"
#include "CollectionController.h"

#include "DialogController.h"

namespace HomeCompa::Flibrary {

namespace {

SimpleModelItems GetSimpleModeItems(const Collections & collections)
{
	SimpleModelItems items;
	items.reserve(collections.size());
	std::ranges::transform(collections, std::back_inserter(items), [] (const CollectionImpl & collection)
	{
		return SimpleModelItem { collection.id, collection.name };
	});

	return items;
}

QString GetFileHash(const QString & fileName)
{
	QFile file(fileName);
	file.open(QIODevice::ReadOnly);
	constexpr auto size = 1024ll * 32;
	const std::unique_ptr<char[]> buf(new char[size]);

	QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);

	while (const auto readSize = file.read(buf.get(), size))
		hash.addData(QByteArray(buf.get(), static_cast<int>(readSize)));

	return hash.result().toHex();
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
	const auto getFile = [&tempDir = *result.first, createFiles](const QString & name)
	{
		auto result = QDir::fromNativeSeparators(QApplication::applicationDirPath() + QDir::separator() + name);
		if (!createFiles || QFile(result).exists())
			return result;

		result = tempDir.filePath(name);
		QFile::copy(":/data/" + name, result);
		return result;
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

struct CollectionController::Impl
{
	IObserver & observer;
	Collections m_collections { CollectionImpl::Deserialize(observer.GetSettings()) };
	QString error;
	QString currentCollectionId { CollectionImpl::GetActive(observer.GetSettings()) };
	PropagateConstPtr<QAbstractItemModel> model { std::unique_ptr<QAbstractItemModel>(CreateSimpleModel(GetSimpleModeItems(m_collections))) };
	PropagateConstPtr<Util::IExecutor> executor { Util::ExecutorFactory::Create(Util::ExecutorImpl::Async)};

	DialogController addModeDialogController;
	DialogController hasUpdateDialogController { [&](QMessageBox::StandardButton button){ return OnHasUpdateDialogButtonClicked(button); } };
	DialogController removeCollectionConfirmDialogController { [&](QMessageBox::StandardButton button){ return OnRemoveCollectionConfirmDialogButtonClicked(button); } };
	DialogController removeDatabaseConfirmDialogController { [&](QMessageBox::StandardButton button){ return OnRemoveDatabaseConfirmDialogButtonClicked(button); } };

	explicit Impl(CollectionController & self, IObserver & observer_)
		: observer(observer_)
		, m_self(self)
	{
		QQmlEngine::setObjectOwnership(model.get(), QQmlEngine::CppOwnership);

		m_collections.erase(std::ranges::remove_if(m_collections, [] (const CollectionImpl & item) { return !QFile::exists(item.database); }).begin(), m_collections.end());

		addModeDialogController.SetVisible(m_collections.empty());
	}

	void OnCollectionsChanged()
	{
		if (m_collections.empty())
		{
			currentCollectionId.clear();
			observer.HandleCurrentCollectionChanged(CollectionImpl());

			addModeDialogController.SetVisible(true);
			return;
		}

		if (const auto * collection = FindCollection(currentCollectionId))
			return observer.HandleCurrentCollectionChanged(*collection);

		currentCollectionId = m_collections.front().id;
		CollectionImpl::SetActive(observer.GetSettings(), currentCollectionId);
		observer.HandleCurrentCollectionChanged(m_collections.front());
	}

	const CollectionImpl * FindCollection(const QString & id) const
	{
		const auto it = std::ranges::find_if(std::as_const(m_collections), [&] (const auto & item) { return item.id == id; });
		return it != std::cend(m_collections) ? &*it : nullptr;
	}

	bool CheckNewCollection(const QString & name, const QString & db, const QString & folder, const bool create)
	{
		if (name.isEmpty())
			return Util::Set(error, QApplication::translate("Error", "Name cannot be empty"), m_self, &CollectionController::ErrorChanged), false;
		if (db.isEmpty())
			return Util::Set(error, QApplication::translate("Error", "Database file name cannot be empty"), m_self, &CollectionController::ErrorChanged), false;
		if (create)
		{
			if (GetInpx(folder).isEmpty())
				return Util::Set(error, QApplication::translate("Error", "Index file (*.inpx) not found"), m_self, &CollectionController::ErrorChanged), false;
			if (QFileInfo(db).suffix().toLower() == "inpx")
				return Util::Set(error, QApplication::translate("Error", "Bad database file extension (.inpx)"), m_self, &CollectionController::ErrorChanged), false;
			if (QFile(db).exists())
				PLOGW << db << " will be rewritten";
		}
		else
		{
			if (!QFile(db).exists())
				return Util::Set(error, QApplication::translate("Error", "Database file not found"), m_self, &CollectionController::ErrorChanged), false;
		}
		if (folder.isEmpty())
			return Util::Set(error, QApplication::translate("Error", "Archive folder name cannot be empty"), m_self, &CollectionController::ErrorChanged), false;
		if (!QDir(folder).exists())
			return Util::Set(error, QApplication::translate("Error", "Archive folder not found"), m_self, &CollectionController::ErrorChanged), false;
		if (QDir(folder).isEmpty())
			return Util::Set(error, QApplication::translate("Error", "Archive folder cannot be empty"), m_self, &CollectionController::ErrorChanged), false;
		if (const auto it = std::ranges::find_if(std::as_const(m_collections), [id = CollectionImpl::GenerateId(db)](const CollectionImpl & item) { return item.id == id; }); it != m_collections.cend())
			return Util::Set(error, QApplication::translate("Error", "This collection has already been added: %1").arg(it->name), m_self, &CollectionController::ErrorChanged), false;

		return true;
	}

private:
	bool OnHasUpdateDialogButtonClicked(const QMessageBox::StandardButton button)
	{
		switch (button)
		{
			case QMessageBox::Yes: return ApplyUpdate(), true;
			case QMessageBox::Discard: return DiscardUpdate(), true;
			default: break;
		}

		return true;
	}

	bool OnRemoveCollectionConfirmDialogButtonClicked(const QMessageBox::StandardButton button)
	{
		if (button == QMessageBox::Yes)
			removeDatabaseConfirmDialogController.SetVisible(true);

		return true;
	}

	bool OnRemoveDatabaseConfirmDialogButtonClicked(const QMessageBox::StandardButton button)
	{
		if (button == QMessageBox::Cancel)
			return true;

		const auto * collection = FindCollection(currentCollectionId);
		if (!collection)
		{
			PLOGE << "Current collection " << currentCollectionId << " not found";
			return true;
		}

		CollectionImpl::Remove(observer.GetSettings(), currentCollectionId);

		QTimer::singleShot(std::chrono::milliseconds(200), [removeDatabase = button == QMessageBox::Yes, db = collection->database]() mutable
		{
			QCoreApplication::exit(1234);
			if (removeDatabase)
			{
				QTimer::singleShot(std::chrono::seconds(1), [db = std::move(db)]()
				{
					for (int i = 0; i < 20 && QFile::exists(db) && !QFile::remove(db); ++i, std::this_thread::sleep_for(std::chrono::milliseconds(50)))
						;
					if (QFile::exists(db))
						PLOGE << "Cannot remove collection database: " << db;
				});
			}
		});

		return true;
	}

	void ApplyUpdate()
	{
		hasUpdateDialogController.SetVisible(false);
		const auto * collection = FindCollection(currentCollectionId);

		emit m_self.ShowLog(true);

		(*executor)({ "Update collection", [this, collection = *collection]() mutable
		{
			auto result = std::function([this] { emit m_self.ShowLog(true); });

			auto [_, ini] = GetIniMap(collection.database, collection.folder, true);

			if (Inpx::UpdateCollection(std::move(ini)))
			{
				result = std::function([] { QCoreApplication::exit(1234); });
			}

			return result;
		} });
	}

	void DiscardUpdate()
	{
		hasUpdateDialogController.SetVisible(false);
		const auto * collection = FindCollection(currentCollectionId);

		(*executor)({ "Update collection", [this, collection = *collection]() mutable
		{
			auto [_, ini] = GetIniMap(collection.database, collection.folder, false);
			const auto it = ini.find(INPX);
			assert(it != ini.end());
			collection.discardedUpdate = GetFileHash(QString::fromStdWString(it->second));
			collection.Serialize(observer.GetSettings());
			return []{};
		} });
	}

private:
	CollectionController & m_self;
};

CollectionController::CollectionController(IObserver & observer, QObject * parent)
	: QObject(parent)
	, m_impl(*this, observer)
{
	Util::ObjectsConnector::registerEmitter(ObjConn::SHOW_LOG, this, SIGNAL(ShowLog(bool)));
	QTimer::singleShot(0, [&] { m_impl->OnCollectionsChanged(); });

	PLOGD << "CollectionController created";
}

CollectionController::~CollectionController()
{
	PLOGD << "CollectionController destroyed";
}

void CollectionController::CheckForUpdate(const CollectionImpl & collection)
{
	(*m_impl->executor)({ "Check inpx for update", [&this_ = *this, collection = collection]() mutable
	{
		auto result = std::function([] {});

		auto [_, ini] = GetIniMap(collection.database, collection.folder, false);
		if (!collection.discardedUpdate.isEmpty())
		{
			const auto it = ini.find(INPX);
			assert(it != ini.end());
			if (GetFileHash(QString::fromStdWString(it->second)) == collection.discardedUpdate)
				return result;
		}

		result = [&this_, hasUpdate = Inpx::CheckUpdateCollection(std::move(ini))]
		{
			this_.m_impl->hasUpdateDialogController.SetVisible(hasUpdate);
		};

		return result;
	} });
}

bool CollectionController::AddCollection(QString name, QString db, QString folder)
{
	if (!m_impl->CheckNewCollection(name, db, folder, false))
		return false;

	folder.replace("\\", "/");
	while (folder.endsWith("\\"))
		folder.resize(folder.size() - 1);

	const CollectionImpl collection(std::move(name), std::move(db), std::move(folder));
	collection.Serialize(m_impl->observer.GetSettings());
	SetCurrentCollectionId(collection.id);

	return true;
}

void CollectionController::CreateCollection(QString name, QString db, QString folder)
{
	if (!QFile(db).open(QIODevice::WriteOnly))
	{
		Util::Set(m_impl->error, QApplication::translate("Error", "No write access to %1").arg(db), *this, &CollectionController::ErrorChanged);
		return m_impl->addModeDialogController.SetVisible(true);
	}

	emit ShowLog(true);

	(*m_impl->executor)({"Create collection", [this_ = this, name = std::move(name), db = std::move(db), folder = std::move(folder)]() mutable
	{
		auto result = std::function([this_] { emit this_->ShowLog(true); });

		auto [_, ini] = GetIniMap(db, folder, true);
		if (Inpx::CreateNewCollection(std::move(ini)))
		{
			result = std::function([this_, name = std::move(name), db = std::move(db), folder = std::move(folder)]() mutable
			{
				this_->AddCollection(std::move(name), std::move(db), std::move(folder));
			});
		}

		return result;
	}});
}

bool CollectionController::CheckCreateCollection(const QString & name, const QString & db, const QString & folder)
{
	return m_impl->CheckNewCollection(name, db, folder, true);
}

QAbstractItemModel * CollectionController::GetModel()
{
	return m_impl->model.get();
}

int CollectionController::CollectionsCount() const noexcept
{
	return static_cast<int>(m_impl->m_collections.size());
}

DialogController * CollectionController::GetAddModeDialogController() noexcept
{
	return &m_impl->addModeDialogController;
}

DialogController * CollectionController::GetHasUpdateDialogController() noexcept
{
	return &m_impl->hasUpdateDialogController;
}

DialogController * CollectionController::GetRemoveCollectionConfirmDialogController() noexcept
{
	return &m_impl->removeCollectionConfirmDialogController;
}

DialogController * CollectionController::GetRemoveDatabaseConfirmDialogController() noexcept
{
	return &m_impl->removeDatabaseConfirmDialogController;
}

const QString & CollectionController::GetCurrentCollectionId() const noexcept
{
	return m_impl->currentCollectionId;
}

const QString & CollectionController::GetError() const noexcept
{
	return m_impl->error;
}

void CollectionController::SetCurrentCollectionId(const QString & id)
{
	CollectionImpl::SetActive(m_impl->observer.GetSettings(), id);
	QTimer::singleShot(std::chrono::milliseconds(200), [] { QCoreApplication::exit(1234); });
}

void CollectionController::SetError(const QString & error)
{
	m_impl->error = error;
	emit ErrorChanged();
}

}
