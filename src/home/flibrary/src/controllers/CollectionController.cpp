#include <ranges>

#include <QApplication>

#include "Collection.h"
#include "CollectionController.h"

#include "util/Settings.h"

namespace HomeCompa::Flibrary {

struct CollectionController::Impl
{
	Observer & observer;
	Collections collections { Collection::Deserialize(observer.GetSettings()) };
	QString currentCollectionId { Collection::GetActive(observer.GetSettings()) };

	explicit Impl(Observer & observer_)
		: observer(observer_)
	{
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

		if (const auto it = std::ranges::find_if(collections, [&] (const auto & item) { return item.id == currentCollectionId; }); it != collections.cend())
			return observer.HandleCurrentCollectionChanged(*it);

		currentCollectionId = collections.front().id;
		observer.HandleCurrentCollectionChanged(collections.front());
	}
};

CollectionController::CollectionController(Observer & observer, QObject * parent)
	: QObject(parent)
	, m_impl(observer)
{
}

CollectionController::~CollectionController() = default;

void CollectionController::AddCollection(QString name, QString db, QString folder)
{
	const Collection collection(std::move(name), std::move(db), std::move(folder));
	collection.Serialize(m_impl->observer.GetSettings());
	collection.SetActive(m_impl->observer.GetSettings());
	QApplication::exit(1234);
}

}
