#include "BaseModel.h"

#include "interface/constants/ModelRole.h"

#include "util/Measure.h"

#include "data/AbstractModelProvider.h"
#include "data/DataItem.h"

using namespace HomeCompa::Flibrary;

BaseModel::BaseModel(const std::shared_ptr<AbstractModelProvider> & modelProvider, QObject * parent)
	: QAbstractItemModel(parent)
	, m_data(modelProvider->GetData())
{
}

BaseModel::~BaseModel() = default;

QVariant BaseModel::data(const QModelIndex & index, const int role) const
{
	if (!index.isValid())
		return {};

	const auto * item = static_cast<DataItem *>(index.internalPointer());
	switch (role)
	{
		case Qt::DisplayRole:
			switch (index.column())
			{
				case BookItem::Column::SeqNumber:
					if (const auto seqNumber = item->GetData(index.column()).toInt(); seqNumber > 0)
						return seqNumber;

					return {};

				case BookItem::Column::Size:
					return Measure::GetSize(item->GetData(index.column()).toULongLong());

				default:
					return item->GetData(index.column());
			}

		case Role::Id:
			return item->GetId();

		default:
			break;
	}

	return {};
}
