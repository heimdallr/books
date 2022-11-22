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

#include "util/constant.h"
#include "util/inpx.h"
#include "util/StrUtil.h"

#include "util/executor.h"
#include "util/executor/factory.h"

#include "constants/ObjectConnectorConstant.h"
#include "Collection.h"
#include "CollectionController.h"

namespace HomeCompa::Flibrary {

SimpleModelItems GetSimpleModeItems(const Collections & collections)
{
	SimpleModelItems items;
	items.reserve(collections.size());
	std::ranges::transform(collections, std::back_inserter(items), [] (const Collection & collection)
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
		hash.addData(buf.get(), static_cast<int>(readSize));

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

using IniMap = std::map<std::wstring, std::wstring>;
using IniMapPair = std::pair<std::unique_ptr<QTemporaryDir>, IniMap>;
IniMapPair GetIniMap(const QString & db, const QString & folder)
{
	IniMapPair result { std::make_unique<QTemporaryDir>(), IniMap{} };
	const auto getFile = [&tempDir = *result.first] (const QString & name)
	{
		auto result = QApplication::applicationDirPath() + name;
		if (QFile(result).exists())
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

	return result;
}


struct CollectionController::Impl
{
	Observer & observer;
	bool addMode { false };
	bool hasUpdate { false };
	QString error;
	Collections collections { Collection::Deserialize(observer.GetSettings()) };
	QString currentCollectionId { Collection::GetActive(observer.GetSettings()) };
	PropagateConstPtr<QAbstractItemModel> model { std::unique_ptr<QAbstractItemModel>(CreateSimpleModel(GetSimpleModeItems(collections))) };
	PropagateConstPtr<Util::Executor> executor { Util::ExecutorFactory::Create(Util::ExecutorImpl::Async, Util::ExecutorInitializer{[]{}, [&]{ emit m_self.ShowLog(true); }, [&]{emit m_self.ShowLog(false);}}) };

	explicit Impl(CollectionController & self, Observer & observer_)
		: observer(observer_)
		, m_self(self)
	{
		QQmlEngine::setObjectOwnership(model.get(), QQmlEngine::CppOwnership);

		collections.erase(std::ranges::remove_if(collections, [] (const Collection & item) { return !QFile::exists(item.database); }).begin(), collections.end());
	}

	void OnCollectionsChanged()
	{
		if (collections.empty())
		{
			currentCollectionId.clear();
			observer.HandleCurrentCollectionChanged(Collection());

			m_self.SetAddMode(true);
			return;
		}

		if (const auto * collection = FindCollection(currentCollectionId))
			return observer.HandleCurrentCollectionChanged(*collection);

		currentCollectionId = collections.front().id;
		Collection::SetActive(observer.GetSettings(), currentCollectionId);
		observer.HandleCurrentCollectionChanged(collections.front());
	}

	const Collection * FindCollection(const QString & id) const
	{
		const auto it = std::ranges::find_if(std::as_const(collections), [&] (const auto & item) { return item.id == id; });
		return it != std::cend(collections) ? &*it : nullptr;
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
		if (const auto it = std::ranges::find_if(std::as_const(collections), [id = Collection::GenerateId(db)](const Collection & item) { return item.id == id; }); it != collections.cend())
			return Util::Set(error, QApplication::translate("Error", "This collection has already been added: %1").arg(it->name), m_self, &CollectionController::ErrorChanged), false;

		return true;
	}

private:
	CollectionController & m_self;
};

CollectionController::CollectionController(Observer & observer, QObject * parent)
	: QObject(parent)
	, m_impl(*this, observer)
{
	Util::ObjectsConnector::registerEmitter(ObjConn::SHOW_LOG, this, SIGNAL(ShowLog(bool)));
	m_impl->OnCollectionsChanged();
}

CollectionController::~CollectionController() = default;

void CollectionController::CheckForUpdate(const Collection & collection)
{
	(*m_impl->executor)({ "Check inpx for update", [&this_ = *this, collection = collection]() mutable
	{
		auto result = std::function([] {});

		auto [_, ini] = GetIniMap(collection.database, collection.folder);
		if (!collection.discardedUpdate.isEmpty())
		{
			const auto it = ini.find(INPX);
			assert(it != ini.end());
			if (GetFileHash(QString::fromStdWString(it->second)) == collection.discardedUpdate)
				return result;
		}

		result = [&this_, hasUpdate = Inpx::CheckUpdateCollection(std::move(ini))]
		{
			if (this_.m_impl->hasUpdate == hasUpdate)
				return;

			this_.m_impl->hasUpdate = hasUpdate;
			emit this_.HasUpdateChanged();
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

	const Collection collection(std::move(name), std::move(db), std::move(folder));
	collection.Serialize(m_impl->observer.GetSettings());
	SetCurrentCollectionId(collection.id);

	return true;
}

void CollectionController::CreateCollection(QString name, QString db, QString folder)
{
	(*m_impl->executor)({"Create collection", [this_ = this, name = std::move(name), db = std::move(db), folder = std::move(folder)]() mutable
	{
		auto result = std::function([] {});

		auto [_, ini] = GetIniMap(db, folder);
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

bool CollectionController::CheckCreateCollection(QString name, QString db, QString folder)
{
	return m_impl->CheckNewCollection(name, db, folder, true);
}

QAbstractItemModel * CollectionController::GetModel()
{
	return m_impl->model.get();
}

void CollectionController::ApplyUpdate()
{
	SetHasUpdate(false);
	const auto * collection = m_impl->FindCollection(m_impl->currentCollectionId);

	(*m_impl->executor)({ "Update collection", [&impl = *m_impl, collection = *collection]() mutable
	{
		auto result = std::function([] {});

		auto [_, ini] = GetIniMap(collection.database, collection.folder);

		if (Inpx::UpdateCollection(std::move(ini)))
		{
			result = std::function([] { QCoreApplication::exit(1234); });
		}

		return result;
	} });
}

void CollectionController::DiscardUpdate()
{
	SetHasUpdate(false);
	const auto * collection = m_impl->FindCollection(m_impl->currentCollectionId);

	(*m_impl->executor)({ "Update collection", [&impl = *m_impl, collection = *collection]() mutable
	{
		auto [_, ini] = GetIniMap(collection.database, collection.folder);
		const auto it = ini.find(INPX);
		assert(it != ini.end());
		collection.discardedUpdate = GetFileHash(QString::fromStdWString(it->second));
		collection.Serialize(impl.observer.GetSettings());
		return []{};
	} });
}

void CollectionController::RemoveCurrentCollection()
{
	Collection::Remove(m_impl->observer.GetSettings(), m_impl->currentCollectionId);
	QTimer::singleShot(std::chrono::milliseconds(200), [] { QCoreApplication::exit(1234); });
}

bool CollectionController::GetAddMode() const noexcept
{
	return m_impl->addMode;
}

bool CollectionController::HasUpdate() const noexcept
{
	return m_impl->hasUpdate;
}

const QString & CollectionController::GetCurrentCollectionId() const noexcept
{
	return m_impl->currentCollectionId;
}

const QString & CollectionController::GetError() const noexcept
{
	return m_impl->error;
}

void CollectionController::SetAddMode(const bool value)
{
	m_impl->addMode = value;
	emit AddModeChanged();
}

void CollectionController::SetHasUpdate(bool value)
{
	m_impl->hasUpdate = value;
	emit HasUpdateChanged();
}

void CollectionController::SetCurrentCollectionId(const QString & id)
{
	Collection::SetActive(m_impl->observer.GetSettings(), id);
	QTimer::singleShot(std::chrono::milliseconds(200), [] { QCoreApplication::exit(1234); });
}

void CollectionController::SetError(const QString & error)
{
	m_impl->error = error;
	emit ErrorChanged();
}

}
