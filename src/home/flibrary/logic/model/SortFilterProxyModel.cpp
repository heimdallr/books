#include "SortFilterProxyModel.h"

#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"

#include "util/SortString.h"

using namespace HomeCompa::Flibrary;

namespace
{

template <typename T>
bool Set(T& oldValue, T&& newValue, const std::function<void()>& f = [] {})
{
	if (oldValue == newValue)
		return false;

	oldValue = std::forward<T>(newValue);
	f();
	return true;
}

}

AbstractSortFilterProxyModel::AbstractSortFilterProxyModel(QObject* parent)
	: QSortFilterProxyModel(parent)
{
}

struct SortFilterProxyModel::Impl final
{
	QString m_filter;
	QString m_languageFilter;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_sourceModel;
	bool m_showRemoved { true };
	QVector<int> m_visibleColumns;

	explicit Impl(const std::shared_ptr<IModelProvider>& modelProvider)
		: m_sourceModel(modelProvider->GetSourceModel())
	{
	}
};

SortFilterProxyModel::SortFilterProxyModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent)
	: AbstractSortFilterProxyModel(parent)
	, m_impl(modelProvider)
{
	QSortFilterProxyModel::setSourceModel(m_impl->m_sourceModel.get());
	setAutoAcceptChildRows(true);
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
				return Set(m_impl->m_filter, value.toString().simplified(), [&] { invalidateFilter(); });

			case Role::VisibleColumns:
				return Set(m_impl->m_visibleColumns, value.value<QVector<int>>(), [&] { invalidateFilter(); });

			case Role::ShowRemovedFilter:
				return Set(m_impl->m_showRemoved, value.toBool(), [&] { invalidateFilter(); });

			case Role::LanguageFilter:
				if (Set(m_impl->m_languageFilter, value.toString().simplified(), [&] { invalidateFilter(); }))
				{
					emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
					return true;
				}

				return false;

			case Role::SortOrder:
			{
				const auto [column, order] = value.value<QPair<int, Qt::SortOrder>>();
				sort(column, order);
				return true;
			}

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
	return true && FilterAcceptsText(itemIndex) && FilterAcceptsLanguage(itemIndex) && FilterAcceptsRemoved(itemIndex);
}

bool SortFilterProxyModel::lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const
{
	const auto lhs = sourceLeft.data(), rhs = sourceRight.data();
	const auto lhsType = lhs.typeId(), rhsType = rhs.typeId();
	return lhsType != rhsType                  ? lhsType < rhsType
	     : lhsType == QMetaType::Type::QString ? (assert(rhsType == QMetaType::Type::QString), Util::QStringWrapper(lhs.toString()) < Util::QStringWrapper(rhs.toString()))
	                                           : QSortFilterProxyModel::lessThan(sourceLeft, sourceRight);
}

bool SortFilterProxyModel::FilterAcceptsText(const QModelIndex& index) const
{
	if (m_impl->m_filter.isEmpty())
		return true;

	if (index.data(Role::Type).value<ItemType>() == ItemType::Navigation)
		return index.data().toString().simplified().contains(m_impl->m_filter, Qt::CaseInsensitive);

	return std::ranges::any_of(m_impl->m_visibleColumns,
	                           [&](const auto n)
	                           {
								   auto value = index.data(Role::Author + BookItem::Remap(n)).toString();
								   return value.simplified().contains(m_impl->m_filter, Qt::CaseInsensitive);
							   });
}

bool SortFilterProxyModel::FilterAcceptsLanguage(const QModelIndex& index) const
{
	return false || m_impl->m_languageFilter.isEmpty() || (index.data(Role::Type).value<ItemType>() == ItemType::Books && index.data(Role::Lang) == m_impl->m_languageFilter);
}

bool SortFilterProxyModel::FilterAcceptsRemoved(const QModelIndex& index) const
{
	return m_impl->m_showRemoved || !index.data(Role::IsRemoved).toBool();
}
