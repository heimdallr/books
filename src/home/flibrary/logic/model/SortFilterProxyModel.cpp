#include "SortFilterProxyModel.h"

#include "interface/constants/ModelRole.h"

#include "data/AbstractModelProvider.h"
#include "interface/constants/Enums.h"

using namespace HomeCompa::Flibrary;

namespace {

template <typename T>
bool Set(T & oldValue, T && newValue, const std::function<void()> & f)
{
	if (oldValue == newValue)
		return false;

	oldValue = std::forward<T>(newValue);
	f();
	return true;
}

}

AbstractSortFilterProxyModel::AbstractSortFilterProxyModel(QObject * parent)
	: QSortFilterProxyModel(parent)
{
}

SortFilterProxyModel::SortFilterProxyModel(const std::shared_ptr<AbstractModelProvider> & modelProvider, QObject * parent)
	: AbstractSortFilterProxyModel(parent)
	, m_sourceModel(modelProvider->GetSourceModel())
{
	QSortFilterProxyModel::setSourceModel(m_sourceModel.get());
	setAutoAcceptChildRows(true);
	setRecursiveFilteringEnabled(true);
}

SortFilterProxyModel::~SortFilterProxyModel() = default;

QVariant SortFilterProxyModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
	return orientation == Qt::Horizontal && role == Qt::DisplayRole && BookItem::Remap(section) == BookItem::Column::Lang && !m_languageFilter.isEmpty()
		? m_languageFilter
		: AbstractSortFilterProxyModel::headerData(section, orientation, role);
}

QVariant SortFilterProxyModel::data(const QModelIndex & index, const int role) const
{
	if (!index.isValid())
	{
		switch (role)
		{
			case Role::Filter:
				return m_filter;

			case Role::LanguageFilter:
				return m_languageFilter;

			default:
				break;
		}
	}

	return QSortFilterProxyModel::data(index, role);
}

bool SortFilterProxyModel::setData(const QModelIndex & index, const QVariant & value, const int role)
{
	if (!index.isValid())
	{
		switch (role)
		{
			case Role::Filter:
				return Set(m_filter, value.toString().simplified(), [&] { invalidateFilter(); });

			case Role::LanguageFilter:
				if (Set(m_languageFilter, value.toString().simplified(), [&] { invalidateFilter(); }))
				{
					emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
					return true;
				}

				return false;

			default:
				break;
		}
	}

	return QSortFilterProxyModel::setData(index, value, role);
}

bool SortFilterProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex & sourceParent) const
{

	const auto itemIndex = m_sourceModel->index(sourceRow, 0, sourceParent);
	assert(itemIndex.isValid());
	return filterAcceptsText(itemIndex) && filterAcceptsLanguage(itemIndex);
}

bool SortFilterProxyModel::filterAcceptsText(const QModelIndex & index) const
{
	if (m_filter.isEmpty())
		return true;

	const auto text = index.data(Role::Type).value<ItemType>() == ItemType::Navigation ? index.data() : index.data(Role::Title);
	return text.toString().simplified().contains(m_filter, Qt::CaseInsensitive);
}

bool SortFilterProxyModel::filterAcceptsLanguage(const QModelIndex & index) const
{
	return false
		|| m_languageFilter.isEmpty()
		|| (index.data(Role::Type).value<ItemType>() == ItemType::Books && index.data(Role::Lang) == m_languageFilter)
		;
}
