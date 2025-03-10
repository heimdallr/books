#include "ModelProvider.h"

// ReSharper disable once CppUnusedIncludeDirective
#include <QString> // for plog

#include <Hypodermic/Container.h>

#include "interface/logic/IScriptController.h"

#include "model/FilteredProxyModel.h"
#include "model/IModelObserver.h"
#include "model/ListModel.h"
#include "model/SortFilterProxyModel.h"
#include "model/TreeModel.h"
#include "model/script/ScriptCommandModel.h"
#include "model/script/ScriptModel.h"
#include "model/script/ScriptSortFilterModel.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

namespace
{

class BooksSearchProxyModel final : public QIdentityProxyModel
{
public:
	explicit BooksSearchProxyModel(std::shared_ptr<QAbstractItemModel> source, QObject* parent = nullptr)
		: QIdentityProxyModel(parent)
		, m_source { std::move(source) }
	{
		QIdentityProxyModel::setSourceModel(m_source.get());
	}

private: // QAbstractItemModel
	QVariant data(const QModelIndex& index, const int role) const override
	{
		if (role != Qt::DisplayRole)
			return QIdentityProxyModel::data(index, role);

		auto list = QIdentityProxyModel::data(index, role).toString().split(' ', Qt::SkipEmptyParts);
		for (auto& item : list)
			if (item.endsWith('*'))
				item.chop(1);

		return list.join(' ');
	}

private:
	std::shared_ptr<QAbstractItemModel> m_source;
};

} // namespace

struct ModelProvider::Impl
{
	Hypodermic::Container& container;
	mutable IDataItem::Ptr data;
	mutable IModelObserver* observer { nullptr };
	mutable std::shared_ptr<QAbstractItemModel> sourceModel;

	explicit Impl(Hypodermic::Container& container)
		: container(container)
	{
	}

	template <typename T>
	std::shared_ptr<QAbstractItemModel> CreateModel(IDataItem::Ptr d, IModelObserver& o) const
	{
		data = std::move(d);
		observer = &o;
		sourceModel = container.resolve<T>();
		sourceModel = container.resolve<AbstractSortFilterProxyModel>();
		return container.resolve<AbstractFilteredProxyModel>();
	}
};

ModelProvider::ModelProvider(Hypodermic::Container& container)
	: m_impl(container)
{
	PLOGV << "ModelProvider created";
}

ModelProvider::~ModelProvider()
{
	PLOGV << "ModelProvider destroyed";
}

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateListModel(IDataItem::Ptr data, IModelObserver& observer) const
{
	return m_impl->CreateModel<ListModel>(std::move(data), observer);
}

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateSearchListModel(IDataItem::Ptr data, IModelObserver& observer) const
{
	auto model = CreateListModel(std::move(data), observer);
	return std::make_shared<BooksSearchProxyModel>(std::move(model));
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

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateTreeModel(IDataItem::Ptr data, IModelObserver& observer) const
{
	return m_impl->CreateModel<TreeModel>(std::move(data), observer);
}

IDataItem::Ptr ModelProvider::GetData() const noexcept
{
	assert(m_impl->data);
	return std::move(m_impl->data);
}

IModelObserver& ModelProvider::GetObserver() const noexcept
{
	assert(m_impl->observer);
	return *m_impl->observer;
}

[[nodiscard]] std::shared_ptr<QAbstractItemModel> ModelProvider::GetSourceModel() const noexcept
{
	assert(m_impl->sourceModel);
	return std::move(m_impl->sourceModel);
}
