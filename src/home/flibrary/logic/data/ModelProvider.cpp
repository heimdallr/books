#include "ModelProvider.h"

#include <Hypodermic/Container.h>

#include "model/FilteredProxyModel.h"
#include "model/IModelObserver.h"
#include "model/ListModel.h"
#include "model/SortFilterProxyModel.h"
#include "model/TreeModel.h"

using namespace HomeCompa::Flibrary;

struct ModelProvider::Impl
{
	Hypodermic::Container & container;
	mutable DataItem::Ptr data { std::shared_ptr<DataItem>() };
	mutable IModelObserver * observer { nullptr };
	mutable std::shared_ptr<QAbstractItemModel> sourceModel;

	explicit Impl(Hypodermic::Container & container)
		: container(container)
	{
	}

	template <typename T>
	std::shared_ptr<QAbstractItemModel> CreateModel(DataItem::Ptr d, IModelObserver & o) const
	{
		data = std::move(d);
		observer = &o;
		sourceModel = container.resolve<T>();
		sourceModel = container.resolve<AbstractSortFilterProxyModel>();
		return container.resolve<AbstractFilteredProxyModel>();
	}
};

ModelProvider::ModelProvider(Hypodermic::Container & container)
	: m_impl(container)
{
}

ModelProvider::~ModelProvider() = default;

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateListModel(DataItem::Ptr data, IModelObserver & observer) const
{
	return m_impl->CreateModel<ListModel>(std::move(data), observer);
}

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateTreeModel(DataItem::Ptr data, IModelObserver & observer) const
{
	return m_impl->CreateModel<TreeModel>(std::move(data), observer);
}

DataItem::Ptr ModelProvider::GetData() const noexcept
{
	assert(m_impl->data);
	return std::move(m_impl->data);
}

IModelObserver & ModelProvider::GetObserver() const noexcept
{
	assert(m_impl->observer);
	return *m_impl->observer;
}

[[nodiscard]] std::shared_ptr<QAbstractItemModel> ModelProvider::GetSourceModel() const noexcept
{
	assert(m_impl->sourceModel);
	return std::move(m_impl->sourceModel);
}
