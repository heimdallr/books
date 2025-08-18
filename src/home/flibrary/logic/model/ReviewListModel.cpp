#include "ReviewListModel.h"

#include "fnd/IsOneOf.h"

#include "interface/constants/ModelRole.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

ReviewListModel::ReviewListModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent)
	: ListModel(modelProvider, parent)
{
	PLOGV << "ReviewListModel created";
}

ReviewListModel::~ReviewListModel()
{
	PLOGV << "ReviewListModel destroyed";
}

int ReviewListModel::columnCount(const QModelIndex& parent) const
{
	const auto* parentItem = parent.isValid() ? static_cast<IDataItem*>(parent.internalPointer()) : m_data.get();
	return parentItem->GetChildCount() ? parentItem->GetChild(0)->GetColumnCount() + (parentItem->GetChild(0)->GetChildCount() ? parentItem->GetChild(0)->GetChild(0)->GetColumnCount() : 0) : 0;
}

QVariant ReviewListModel::data(const QModelIndex& index, const int role) const
{
	const auto* item = GetInternalPointer(index);
	if (item->To<ReviewItem>())
	{
		switch (role)
		{
			case Qt::DisplayRole:
			case Qt::ToolTipRole:
				return GetValue(*item, index.column() - item->GetChild(0)->GetColumnCount());

			case Role::Remap:
				return index.column();

			default:
				break;
		}
	}

	return BaseModel::data(index, role);
}

IDataItem* ReviewListModel::GetInternalPointer(const QModelIndex& index) const
{
	auto* reviewItem = BaseModel::GetInternalPointer(index);
	auto bookItem = reviewItem->GetChild(0);
	return index.column() < bookItem->GetColumnCount() ? bookItem.get() : reviewItem;
}
