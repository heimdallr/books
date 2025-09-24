#include "ModelProvider.h"

#include <Hypodermic/Container.h>

#include "fnd/FindPair.h"

#include "interface/constants/Enums.h"
#include "interface/logic/ILibRateProvider.h"

#include "model/AuthorReviewModel.h"
#include "model/AuthorsModel.h"
#include "model/FilterListModel.h"
#include "model/FilterTreeModel.h"
#include "model/FilteredProxyModel.h"
#include "model/ListModel.h"
#include "model/ReviewListModel.h"
#include "model/ReviewTreeModel.h"
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
	mutable std::shared_ptr<QAbstractItemModel> sourceModel;
	NavigationMode navigationMode { NavigationMode::Unknown };

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
	std::shared_ptr<QAbstractItemModel> CreateModel(IDataItem::Ptr d, const bool autoAcceptChildRows) const
	{
		data = std::move(d);
		sourceModel = container.resolve<T>();
		sourceModel = CreateSortFilterProxyModel(autoAcceptChildRows); //-V519
		return container.resolve<AbstractFilteredProxyModel>();
	}

	std::shared_ptr<QAbstractItemModel> CreateBookListModel(IDataItem::Ptr dataItem, const bool autoAcceptChildRows) const
	{
		using ModelCreator = std::shared_ptr<QAbstractItemModel> (Impl::*)(IDataItem::Ptr, bool) const;
		static constexpr std::pair<NavigationMode, ModelCreator> creators[] {
			{ NavigationMode::Reviews, &Impl::CreateModel<ReviewListModel> },
		};

		const auto modelCreator = FindSecond(creators, navigationMode, &Impl::CreateModel<ListModel>);
		return std::invoke(modelCreator, *this, std::move(dataItem), autoAcceptChildRows);
	}

	std::shared_ptr<QAbstractItemModel> CreateBookTreeModel(IDataItem::Ptr dataItem, const bool autoAcceptChildRows) const
	{
		using ModelCreator = std::shared_ptr<QAbstractItemModel> (Impl::*)(IDataItem::Ptr, bool) const;
		static constexpr std::pair<NavigationMode, ModelCreator> creators[] {
			{ NavigationMode::Reviews, &Impl::CreateModel<ReviewTreeModel> },
		};

		const auto modelCreator = FindSecond(creators, navigationMode, &Impl::CreateModel<TreeModel>);
		return std::invoke(modelCreator, *this, std::move(dataItem), autoAcceptChildRows);
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

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateListModel(IDataItem::Ptr data, const bool autoAcceptChildRows) const
{
	return m_impl->CreateModel<ListModel>(std::move(data), autoAcceptChildRows);
}

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateAuthorsListModel(IDataItem::Ptr data, const bool autoAcceptChildRows) const
{
	return m_impl->CreateModel<AuthorsModel>(std::move(data), autoAcceptChildRows);
}

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateSearchListModel(IDataItem::Ptr data, const bool autoAcceptChildRows) const
{
	auto model = CreateListModel(std::move(data), autoAcceptChildRows);
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

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateAuthorReviewModel() const
{
	return m_impl->container.resolve<AuthorReviewModel>();
}

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateFilterListModel(IDataItem::Ptr data) const
{
	m_impl->data = std::move(data);
	m_impl->sourceModel = m_impl->container.resolve<FilterListModel>();
	return m_impl->CreateSortFilterProxyModel(false);
}

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateFilterTreeModel(IDataItem::Ptr data) const
{
	m_impl->data = std::move(data);
	m_impl->sourceModel = m_impl->container.resolve<FilterTreeModel>();
	return m_impl->CreateSortFilterProxyModel(false);
}

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateTreeModel(IDataItem::Ptr data, const bool autoAcceptChildRows) const
{
	return m_impl->CreateModel<TreeModel>(std::move(data), autoAcceptChildRows);
}

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateBookListModel(IDataItem::Ptr data, const bool autoAcceptChildRows) const
{
	return m_impl->CreateBookListModel(std::move(data), autoAcceptChildRows);
}

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateBookTreeModel(IDataItem::Ptr data, const bool autoAcceptChildRows) const
{
	return m_impl->CreateBookTreeModel(std::move(data), autoAcceptChildRows);
}

IDataItem::Ptr ModelProvider::GetData() const noexcept
{
	assert(m_impl->data);
	return std::move(m_impl->data);
}

[[nodiscard]] std::shared_ptr<QAbstractItemModel> ModelProvider::GetSourceModel() const noexcept
{
	assert(m_impl->sourceModel);
	return std::move(m_impl->sourceModel);
}

std::shared_ptr<const ILibRateProvider> ModelProvider::GetLibRateProvider() const
{
	return m_impl->container.resolve<ILibRateProvider>();
}

void ModelProvider::OnModeChanged(const int index)
{
	m_impl->navigationMode = static_cast<NavigationMode>(index);
}

void ModelProvider::OnModelChanged(QAbstractItemModel* /*model*/)
{
}

void ModelProvider::OnContextMenuTriggered(const QString& /*id*/, const IDataItem::Ptr& /*item*/)
{
}
