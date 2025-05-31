#include "ModelProvider.h"

#include <Hypodermic/Container.h>

#include "interface/logic/ILibRateProvider.h"

#include "model/AuthorsModel.h"
#include "model/FilteredProxyModel.h"
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

	std::shared_ptr<QAbstractItemModel> CreateSortFilterProxyModel(const bool autoAcceptChildRows) const
	{
		auto model = container.resolve<AbstractSortFilterProxyModel>();
		if (autoAcceptChildRows)
			model->setAutoAcceptChildRows(autoAcceptChildRows);
		return model;
	}

	template <typename T>
	std::shared_ptr<QAbstractItemModel> CreateModel(IDataItem::Ptr d, IModelObserver& o, const bool autoAcceptChildRows) const
	{
		data = std::move(d);
		observer = &o;
		sourceModel = container.resolve<T>();
		sourceModel = CreateSortFilterProxyModel(autoAcceptChildRows); //-V519
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

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateListModel(IDataItem::Ptr data, IModelObserver& observer, const bool autoAcceptChildRows) const
{
	return m_impl->CreateModel<ListModel>(std::move(data), observer, autoAcceptChildRows);
}

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateAuthorsListModel(IDataItem::Ptr data, IModelObserver& observer, const bool autoAcceptChildRows) const
{
	return m_impl->CreateModel<AuthorsModel>(std::move(data), observer, autoAcceptChildRows);
}

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateSearchListModel(IDataItem::Ptr data, IModelObserver& observer, const bool autoAcceptChildRows) const
{
	auto model = CreateListModel(std::move(data), observer, autoAcceptChildRows);
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

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateTreeModel(IDataItem::Ptr data, IModelObserver& observer, const bool autoAcceptChildRows) const
{
	return m_impl->CreateModel<TreeModel>(std::move(data), observer, autoAcceptChildRows);
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

std::shared_ptr<const ILibRateProvider> ModelProvider::GetLibRateProvider() const noexcept
{
	return m_impl->container.resolve<ILibRateProvider>();
}
