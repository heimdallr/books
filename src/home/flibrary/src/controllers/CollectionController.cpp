#include <QAbstractItemModel>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QQmlEngine>

#include "fnd/algorithm.h"
#include "models/SimpleModel.h"

#include "Collection.h"
#include "CollectionController.h"

namespace HomeCompa::Flibrary {

SimpleModeItems GetSimpleModeItems(const Collections & collections)
{
	SimpleModeItems items;
	items.reserve(collections.size());
	std::ranges::transform(collections, std::back_inserter(items), [] (const Collection & collection)
	{
		return SimpleModeItem { collection.id, collection.name };
	});

	return items;
}

struct CollectionController::Impl
{
	Observer & observer;
	QString error;
	Collections collections { Collection::Deserialize(observer.GetSettings()) };
	QString currentCollectionId { Collection::GetActive(observer.GetSettings()) };
	PropagateConstPtr<QAbstractItemModel> model { std::unique_ptr<QAbstractItemModel>(CreateSimpleModel(GetSimpleModeItems(collections))) };

	explicit Impl(const CollectionController & self, Observer & observer_)
		: observer(observer_)
		, m_self(self)
	{
		QQmlEngine::setObjectOwnership(model.get(), QQmlEngine::CppOwnership);
		OnCollectionsChanged();
	}

	void OnCollectionsChanged()
	{
		if (collections.empty())
		{
			currentCollectionId.clear();
			observer.HandleCurrentCollectionChanged(Collection());
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
			if (QFile(db).exists())
				return Util::Set(error, QApplication::translate("Error", "Database file already exists"), m_self, &CollectionController::ErrorChanged), false;
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
	const CollectionController & m_self;
};

CollectionController::CollectionController(Observer & observer, QObject * parent)
	: QObject(parent)
	, m_impl(*this, observer)
{
}

CollectionController::~CollectionController() = default;

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

bool CollectionController::CreateCollection(QString name, QString db, QString folder)
{
	if (!m_impl->CheckNewCollection(name, db, folder, true))
		return false;

	return true;
}

QAbstractItemModel * CollectionController::GetModel()
{
	return m_impl->model.get();
}

void CollectionController::RemoveCurrentCollection()
{
	Collection::Remove(m_impl->observer.GetSettings(), m_impl->currentCollectionId);
	QApplication::exit(1234);
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
	Collection::SetActive(m_impl->observer.GetSettings(), id);
	QApplication::exit(1234);
}

void CollectionController::SetError(const QString & error)
{
	m_impl->error = error;
	emit ErrorChanged();
}

}
