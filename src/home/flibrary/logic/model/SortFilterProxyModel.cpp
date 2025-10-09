#include "SortFilterProxyModel.h"

#include "fnd/algorithm.h"

#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"

#include "util/SortString.h"

using namespace HomeCompa;
using namespace Flibrary;

AbstractSortFilterProxyModel::AbstractSortFilterProxyModel(QObject* parent)
	: QSortFilterProxyModel(parent)
{
}

struct SortFilterProxyModel::Impl final
{
	QString                                                m_filter;
	QString                                                m_languageFilter;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_sourceModel;
	bool                                                   m_showRemoved { true };
	bool                                                   m_navigationFiltered { false };
	bool                                                   m_uniFilterEnabled { false };
	QVector<int>                                           m_visibleColumns;
	std::vector<std::pair<int, Qt::SortOrder>>             sort;

	explicit Impl(const IModelProvider& modelProvider)
		: m_sourceModel(modelProvider.GetSourceModel())
	{
	}
};

SortFilterProxyModel::SortFilterProxyModel(const std::shared_ptr<const IModelProvider>& modelProvider, QObject* parent)
	: AbstractSortFilterProxyModel(parent)
	, m_impl(*modelProvider)
{
	QSortFilterProxyModel::setSourceModel(m_impl->m_sourceModel.get());
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
			return m_impl->m_languageFilter.isEmpty() ? QVariant {} : m_impl->m_languageFilter;

		case Qt::DecorationRole:
			return m_impl->m_languageFilter.isEmpty() ? QString(":/icons/language.svg") : QVariant {};

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
				return m_impl->m_filter;

			case Role::LanguageFilter:
				return m_impl->m_languageFilter;

			default:
				break;
		}
	}

	return QSortFilterProxyModel::data(index, role);
}

bool SortFilterProxyModel::setData(const QModelIndex& index, const QVariant& value, const int role)
{
	if (!index.isValid())
	{
		switch (role)
		{
			case Role::TextFilter:
				return Util::Set(
					m_impl->m_filter,
					value.toString().simplified(),
					[this] {
						beginFilterChange();
					},
					[this] {
						endFilterChange(Direction::Rows);
					}
				);

			case Role::VisibleColumns:
				return Util::Set(
					m_impl->m_visibleColumns,
					value.value<QVector<int>>(),
					[this] {
						beginFilterChange();
					},
					[this] {
						endFilterChange(Direction::Rows);
					}
				);

			case Role::ShowRemovedFilter:
				return Util::Set(
					m_impl->m_showRemoved,
					value.toBool(),
					[this] {
						beginFilterChange();
					},
					[this] {
						endFilterChange(Direction::Rows);
					}
				);

			case Role::NavigationItemFiltered:
				return Util::Set(
					m_impl->m_navigationFiltered,
					value.toBool(),
					[this] {
						beginFilterChange();
					},
					[this] {
						endFilterChange(Direction::Rows);
					}
				);

			case Role::UniFilterEnabled:
				return Util::Set(
					m_impl->m_uniFilterEnabled,
					value.toBool(),
					[this] {
						beginFilterChange();
					},
					[this] {
						endFilterChange(Direction::Rows);
					}
				);

			case Role::UniFilterChanged:
				return beginFilterChange(), endFilterChange(Direction::Rows), true;

			case Role::LanguageFilter:
				if (Util::Set(
						m_impl->m_languageFilter,
						value.toString().simplified(),
						[this] {
							beginFilterChange();
						},
						[this] {
							endFilterChange(Direction::Rows);
						}
					))
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

			default:
				break;
		}
	}

	return QSortFilterProxyModel::setData(index, value, role);
}

bool SortFilterProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
	const auto itemIndex = m_impl->m_sourceModel->index(sourceRow, 0, sourceParent);
	assert(itemIndex.isValid());
	return FilterAcceptsRemoved(itemIndex) && FilterAcceptsFlags(itemIndex) && FilterAcceptsLanguage(itemIndex) && FilterAcceptsText(itemIndex);
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
	const auto lhs = sourceLeft.data(), rhs = sourceRight.data();
	const auto lhsType = lhs.typeId(), rhsType = rhs.typeId();
	return lhsType != rhsType ? lhsType < rhsType
	     : lhsType == QMetaType::Type::QString
	         ? (assert(rhsType == QMetaType::Type::QString), Util::QStringWrapper::Compare(Util::QStringWrapper { lhs.toString() }, Util::QStringWrapper { rhs.toString() }, emptyStringWeight))
	         : QSortFilterProxyModel::lessThan(sourceLeft, sourceRight);
}

bool SortFilterProxyModel::FilterAcceptsText(const QModelIndex& index) const
{
	if (m_impl->m_filter.isEmpty())
		return true;

	if (index.data(Role::Type).value<ItemType>() == ItemType::Navigation)
		return index.data().toString().simplified().contains(m_impl->m_filter, Qt::CaseInsensitive);

	return std::ranges::any_of(m_impl->m_visibleColumns, [&](const auto n) {
		auto value = index.data(Role::Author + BookItem::Remap(n)).toString();
		return value.simplified().contains(m_impl->m_filter, Qt::CaseInsensitive);
	});
}

bool SortFilterProxyModel::FilterAcceptsRemoved(const QModelIndex& index) const
{
	return m_impl->m_showRemoved || !index.data(Role::IsRemoved).toBool();
}

bool SortFilterProxyModel::FilterAcceptsFlags(const QModelIndex& index) const
{
	if (!m_impl->m_uniFilterEnabled)
		return true;

	const auto flags = index.data(Role::Flags).value<IDataItem::Flags>();
	return !(flags & IDataItem::Flags::Filtered) || (index.data(Role::Type).value<ItemType>() == ItemType::Books && m_impl->m_navigationFiltered && !(flags & IDataItem::Flags::Multiple));
}

bool SortFilterProxyModel::FilterAcceptsLanguage(const QModelIndex& index) const
{
	return m_impl->m_languageFilter.isEmpty() || index.data(Role::Type).value<ItemType>() == ItemType::Navigation || index.data(Role::Lang).toString() == m_impl->m_languageFilter;
}
