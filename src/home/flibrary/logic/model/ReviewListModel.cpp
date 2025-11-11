#include "ReviewListModel.h"

#include "interface/localization.h"
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

QVariant ReviewListModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
	if (orientation != Qt::Horizontal)
		return ListModel::headerData(section, orientation, role);

	const auto getHeader = [&] {
		return section < m_data->GetColumnCount() ? m_data->GetData(section) : m_data->GetRawData(section - m_data->GetColumnCount() + BookItem::Column::Last);
	};

	switch (role)
	{
		case Qt::DisplayRole:
		case Role::HeaderTitle:
			return Loc::Tr(Loc::Ctx::BOOK, getHeader().toUtf8().data());

		case Role::HeaderName:
			return getHeader();

		default:
			break;
	}

	return ListModel::headerData(section, orientation, role);
}

int ReviewListModel::columnCount(const QModelIndex& /*parent*/) const
{
	if (!m_data->GetChildCount())
		return 0;

	const auto reviewItem = m_data->GetChild(0);
	auto       count      = reviewItem->GetColumnCount() + m_data->GetColumnCount();
	return count;
}

QVariant ReviewListModel::data(const QModelIndex& index, const int role) const
{
	if (!index.isValid())
		return ListModel::data(index, role);

	const auto* item = GetInternalPointer(index);
	if (item->To<ReviewItem>())
	{
		switch (role)
		{
			case Qt::DisplayRole:
			case Qt::ToolTipRole:
				return item->GetData(index.column() - item->GetChild(0)->GetColumnCount());

			case Role::Remap:
				return index.column();

			default:
				break;
		}
	}

	return ListModel::data(index, role);
}

IDataItem* ReviewListModel::GetInternalPointer(const QModelIndex& index) const
{
	auto* reviewItem = BaseModel::GetInternalPointer(index);
	auto  bookItem   = reviewItem->GetChild(0);
	return index.column() < bookItem->GetColumnCount() ? bookItem.get() : reviewItem;
}
