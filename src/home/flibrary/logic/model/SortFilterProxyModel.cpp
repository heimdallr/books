#include "SortFilterProxyModel.h"

#include "fnd/algorithm.h"

#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"
#include "interface/logic/IModelSorter.h"

#include "util/SortString.h"

using namespace HomeCompa;
using namespace Flibrary;

AbstractSortFilterProxyModel::AbstractSortFilterProxyModel(QObject* parent)
	: QSortFilterProxyModel(parent)
{
}

struct SortFilterProxyModel::Impl final : IModelSorter
{
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> sourceModel;
	QString                                                filter;
	QString                                                languageFilter;
	bool                                                   showRemoved { true };
	bool                                                   navigationFiltered { false };
	bool                                                   uniFilterEnabled { false };
	QVector<int>                                           visibleColumns;
	std::vector<std::pair<int, Qt::SortOrder>>             sort;
	const IModelSorter*                                    modelSorter { this };
	std::optional<int>                                     minimumRate;
	std::optional<int>                                     maximumRate;
	bool                                                   hideUnrated { false };

	Impl(SortFilterProxyModel& self, const IModelProvider& modelProvider)
		: sourceModel { modelProvider.GetSourceModel() }
		, m_self { self }
	{
		m_self.QSortFilterProxyModel::setSourceModel(sourceModel.get());
	}

private: // IModelSorter
	bool LessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight, const int emptyStringWeight) const override
	{
		const auto lhs = sourceLeft.data(), rhs = sourceRight.data();
		if (lhs.isValid() && rhs.isValid())
		{
			const auto lhsType = lhs.typeId(), rhsType = rhs.typeId();
			return lhsType != rhsType ? lhsType < rhsType
			     : lhsType == QMetaType::Type::QString
			         ? (assert(rhsType == QMetaType::Type::QString), Util::QStringWrapper::Compare(Util::QStringWrapper { lhs.toString() }, Util::QStringWrapper { rhs.toString() }, emptyStringWeight))
			         : m_self.QSortFilterProxyModel::lessThan(sourceLeft, sourceRight);
		}

		return (lhs.isValid() ? 0 : emptyStringWeight) < (rhs.isValid() ? 0 : emptyStringWeight);
	}

private:
	SortFilterProxyModel& m_self;
};

SortFilterProxyModel::SortFilterProxyModel(const std::shared_ptr<const IModelProvider>& modelProvider, QObject* parent)
	: AbstractSortFilterProxyModel(parent)
	, m_impl(*this, *modelProvider)
{
	setRecursiveFilteringEnabled(true);
}

SortFilterProxyModel::~SortFilterProxyModel() = default;

QVariant SortFilterProxyModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
	if (!(orientation == Qt::Horizontal && BookItem::Remap(section) == BookItem::Column::Lang))
		return AbstractSortFilterProxyModel::headerData(section, orientation, role);

	switch (role)
	{
		case Qt::DisplayRole:
			return m_impl->languageFilter.isEmpty() ? QVariant {} : m_impl->languageFilter;

		case Qt::DecorationRole:
			return m_impl->languageFilter.isEmpty() ? QString(":/icons/language.svg") : QVariant {};

		default:
			break;
	}

	return AbstractSortFilterProxyModel::headerData(section, orientation, role);
}

QVariant SortFilterProxyModel::data(const QModelIndex& index, const int role) const
{
	if (!index.isValid())
	{
		switch (role)
		{
			case Role::TextFilter:
				return m_impl->filter;

			case Role::LanguageFilter:
				return m_impl->languageFilter;

			default:
				break;
		}
	}

	return QSortFilterProxyModel::data(index, role);
}

bool SortFilterProxyModel::setData(const QModelIndex& index, const QVariant& value, const int role)
{
	if (index.isValid())
		return QSortFilterProxyModel::setData(index, value, role);

	const auto setFilter = [this]<typename T>(T& oldValue, T&& newValue) {
		return Util::Set(
			oldValue,
			std::forward<T>(newValue),
			[this] {
				beginFilterChange();
			},
			[this] {
				endFilterChange(Direction::Rows);
			}
		);
	};

	switch (role)
	{
		case Role::TextFilter:
			return setFilter(m_impl->filter, value.toString().simplified());

		case Role::VisibleColumns:
			return setFilter(m_impl->visibleColumns, value.value<QVector<int>>());

		case Role::ShowRemovedFilter:
			return setFilter(m_impl->showRemoved, value.toBool());

		case Role::NavigationItemFiltered:
			return setFilter(m_impl->navigationFiltered, value.toBool());

		case Role::UniFilterEnabled:
			return setFilter(m_impl->uniFilterEnabled, value.toBool());

		case Role::UniFilterChanged:
			return beginFilterChange(), endFilterChange(Direction::Rows), true;

		case Role::UniFilterHideUnrated:
			return setFilter(m_impl->hideUnrated, value.toBool());

		case Role::UniFilterMinimumRate:
			return setFilter(m_impl->minimumRate, value.isValid() ? std::optional { value.toInt() } : std::nullopt);

		case Role::UniFilterMaximumRate:
			return setFilter(m_impl->maximumRate, value.isValid() ? std::optional { value.toInt() } : std::nullopt);

		case Role::LanguageFilter:
			if (setFilter(m_impl->languageFilter, value.toString().simplified()))
			{
				emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
				return true;
			}

			return false;

		case Role::SortOrder:
			m_impl->sort = value.value<std::vector<std::pair<int, Qt::SortOrder>>>();
			QSortFilterProxyModel::sort(m_impl->sort.empty() ? -1 : 0);
			invalidate();
			return true;

		case Role::ModelSorter:
		{
			const auto* modelSorter = value.value<const IModelSorter*>();
			if (!modelSorter)
				modelSorter = m_impl.get();

			return Util::Set(m_impl->modelSorter, modelSorter) && (invalidate(), true);
		}

		default:
			break;
	}

	return QSortFilterProxyModel::setData(index, value, role);
}

bool SortFilterProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
	const auto itemIndex = m_impl->sourceModel->index(sourceRow, 0, sourceParent);
	assert(itemIndex.isValid());
	return itemIndex.data(Role::ChildCount).toInt() == 0 && FilterAcceptsRemoved(itemIndex) && FilterAcceptsFlags(itemIndex) && FilterAcceptsLanguage(itemIndex) && FilterAcceptsText(itemIndex)
	    && FilterAcceptsRate(itemIndex);
}

bool SortFilterProxyModel::lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const
{
	if (m_impl->sort.empty())
		return false;

	for (const auto& [column, order] : m_impl->sort)
	{
		assert(column < sourceModel()->columnCount());
		const auto lhs = sourceModel()->index(sourceLeft.row(), column, sourceLeft.parent());
		const auto rhs = sourceModel()->index(sourceRight.row(), column, sourceRight.parent());

		if (order == Qt::AscendingOrder)
		{
			if (lessThanImpl(lhs, rhs))
				return true;

			if (lessThanImpl(rhs, lhs))
				return false;

			continue;
		}

		if (lessThanImpl(rhs, lhs, std::numeric_limits<int>::min()))
			return true;

		if (lessThanImpl(lhs, rhs, std::numeric_limits<int>::min()))
			return false;
	}

	return false;
}

bool SortFilterProxyModel::lessThanImpl(const QModelIndex& sourceLeft, const QModelIndex& sourceRight, const int emptyStringWeight) const
{
	return m_impl->modelSorter->LessThan(sourceLeft, sourceRight, emptyStringWeight);
}

bool SortFilterProxyModel::FilterAcceptsText(const QModelIndex& index) const
{
	if (m_impl->filter.isEmpty())
		return true;

	if (index.data(Role::Type).value<ItemType>() == ItemType::Navigation)
		return index.data().toString().simplified().contains(m_impl->filter, Qt::CaseInsensitive);

	return std::ranges::any_of(m_impl->visibleColumns, [&](const auto n) {
		auto value = index.data(Role::Author + BookItem::Remap(n)).toString();
		return value.simplified().contains(m_impl->filter, Qt::CaseInsensitive);
	});
}

bool SortFilterProxyModel::FilterAcceptsRemoved(const QModelIndex& index) const
{
	return m_impl->showRemoved || !index.data(Role::IsRemoved).toBool();
}

bool SortFilterProxyModel::FilterAcceptsFlags(const QModelIndex& index) const
{
	if (!m_impl->uniFilterEnabled)
		return true;

	const auto flags = index.data(Role::Flags).value<IDataItem::Flags>();
	return !(flags & IDataItem::Flags::Filtered) || (index.data(Role::Type).value<ItemType>() == ItemType::Books && m_impl->navigationFiltered && !(flags & IDataItem::Flags::Multiple));
}

bool SortFilterProxyModel::FilterAcceptsLanguage(const QModelIndex& index) const
{
	return m_impl->languageFilter.isEmpty() || index.data(Role::Lang).toString() == m_impl->languageFilter;
}

bool SortFilterProxyModel::FilterAcceptsRate(const QModelIndex& index) const
{
	if (!m_impl->uniFilterEnabled || !(m_impl->hideUnrated || !!m_impl->minimumRate || !!m_impl->maximumRate))
		return true;

	auto rateVar = index.data(Role::LibRate);
	if (!rateVar.isValid())
	{
		rateVar = index.data(Role::UserRate);
		if (!rateVar.isValid() && m_impl->hideUnrated)
			return false;
	}

	bool       ok   = false;
	const auto rate = rateVar.toInt(&ok);

	return ok ? (!m_impl->minimumRate || rate >= *m_impl->minimumRate) && (!m_impl->maximumRate || rate <= *m_impl->maximumRate) : !m_impl->hideUnrated;
}
