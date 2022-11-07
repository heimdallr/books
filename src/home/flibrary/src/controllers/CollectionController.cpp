#include <QAbstractItemModel>
#include <QApplication>
#include <QQmlEngine>

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
	Collections collections { Collection::Deserialize(observer.GetSettings()) };
	QString currentCollectionId { Collection::GetActive(observer.GetSettings()) };
	PropagateConstPtr<QAbstractItemModel> model { std::unique_ptr<QAbstractItemModel>(CreateSimpleModel(GetSimpleModeItems(collections))) };

	explicit Impl(Observer & observer_)
		: observer(observer_)
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
	SetCurrentCollectionId(collection.id);
}

QAbstractItemModel * CollectionController::GetModel()
{
	return m_impl->model.get();
}

const QString & CollectionController::GetCurrentCollectionId() const noexcept
{
	return m_impl->currentCollectionId;
}

void CollectionController::SetCurrentCollectionId(const QString & id)
{
	Collection::SetActive(m_impl->observer.GetSettings(), id);
	emit CurrentCollectionIdChanged();
	QApplication::exit(1234);
}

}
