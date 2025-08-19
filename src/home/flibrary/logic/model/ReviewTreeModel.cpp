#include "ReviewTreeModel.h"

#include "interface/constants/ModelRole.h"

#include "util/localization.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

ReviewTreeModel::ReviewTreeModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent)
	: TreeModel(modelProvider, parent)
{
	PLOGV << "ReviewTreeModel created";
}

ReviewTreeModel::~ReviewTreeModel()
{
	PLOGV << "ReviewTreeModel destroyed";
}

QVariant ReviewTreeModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
	if (orientation != Qt::Horizontal)
		return {};

	switch (role)
	{
		case Qt::DisplayRole:
		case Role::HeaderTitle:
		{
			const auto data = section < m_data->GetColumnCount() ? m_data->GetData(section) : m_data->GetRawData(section - m_data->GetColumnCount() + BookItem::Column::Last + 1);
			return Loc::Tr(Loc::Ctx::BOOK, data.toUtf8().data());
		}

		default:
			break;
	}

	return {};
}

int ReviewTreeModel::rowCount(const QModelIndex& parent) const
{
	if (parent.column() > 0)
		return 0;

	const auto* parentItem = parent.isValid() ? static_cast<IDataItem*>(parent.internalPointer()) : m_data.get();
	return parentItem->To<ReviewItem>() ? 0 : static_cast<int>(parentItem->GetChildCount());
}

int ReviewTreeModel::columnCount(const QModelIndex& /*parent*/) const
{
	assert(m_data->GetChildCount());
	const auto reviewerItem = m_data->GetChild(0);
	assert(reviewerItem->GetChildCount());
	const auto reviewItem = reviewerItem->GetChild(0);
	auto count = reviewItem->GetColumnCount() + m_data->GetColumnCount() - 1;
	return count;
}

QVariant ReviewTreeModel::data(const QModelIndex& index, const int role) const
{
	const auto* item = GetInternalPointer(index);
	if (item->To<ReviewItem>())
	{
		switch (role)
		{
			case Qt::DisplayRole:
			case Qt::ToolTipRole:
				return GetValue(*item, index.column() - item->GetChild(0)->GetColumnCount() + 1);

			case Role::Remap:
				return index.column();

			default:
				break;
		}
	}

	return TreeModel::data(index, role);
}

IDataItem* ReviewTreeModel::GetInternalPointer(const QModelIndex& index) const
{
	if (!index.isValid())
		return m_data.get();

	auto* item = BaseModel::GetInternalPointer(index);
	if (item->To<NavigationItem>())
		return item;

	const auto bookItem = item->GetChild(0);
	return index.column() < bookItem->GetColumnCount() ? bookItem.get() : item;
}
