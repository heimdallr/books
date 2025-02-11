#include "ModelProvider.h"

// ReSharper disable once CppUnusedIncludeDirective
#include <QString> // for plog

#include <Hypodermic/Container.h>
#include <plog/Log.h>

#include "interface/logic/IScriptController.h"

#include "model/FilteredProxyModel.h"
#include "model/IModelObserver.h"
#include "model/ListModel.h"
#include "model/SortFilterProxyModel.h"
#include "model/TreeModel.h"
#include "model/script/ScriptCommandModel.h"
#include "model/script/ScriptModel.h"
#include "model/script/ScriptSortFilterModel.h"

using namespace HomeCompa::Flibrary;

struct ModelProvider::Impl
{
	Hypodermic::Container & container;
	mutable IDataItem::Ptr data;
	mutable IModelObserver * observer { nullptr };
	mutable std::shared_ptr<QAbstractItemModel> sourceModel;

	explicit Impl(Hypodermic::Container & container)
		: container(container)
	{
	}

	template <typename T>
	std::shared_ptr<QAbstractItemModel> CreateModel(IDataItem::Ptr d, IModelObserver & o) const
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
	PLOGV << "ModelProvider created";
}

ModelProvider::~ModelProvider()
{
	PLOGV << "ModelProvider destroyed";
}

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateListModel(IDataItem::Ptr data, IModelObserver & observer) const
{
	return m_impl->CreateModel<ListModel>(std::move(data), observer);
}

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateScriptModel() const
{
	m_impl->sourceModel = m_impl->container.resolve<ScriptModel>();
	return m_impl->container.resolve<ScriptSortFilterModel>();
}

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateScriptCommandModel() const
{
	m_impl->sourceModel = m_impl->container.resolve<ScriptCommandModel>();
	return m_impl->container.resolve<ScriptSortFilterModel>();
}

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateTreeModel(IDataItem::Ptr data, IModelObserver & observer) const
{
	return m_impl->CreateModel<TreeModel>(std::move(data), observer);
}

IDataItem::Ptr ModelProvider::GetData() const noexcept
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
