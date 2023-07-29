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

QVariant BaseModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
	if (!(orientation == Qt::Horizontal && role == Qt::DisplayRole))
		return {};

	return m_data->GetData(section);
}

QVariant BaseModel::data(const QModelIndex & index, const int role) const
{
	if (!index.isValid())
		return {};

	const auto * item = static_cast<DataItem *>(index.internalPointer());
	switch (role)
	{
		case Qt::DisplayRole:
			switch (item->RemapColumn(index.column()))
			{
				case BookItem::Column::SeqNumber:
					if (const auto seqNumber = item->GetData(index.column()).toInt(); seqNumber > 0)
						return seqNumber;

					return {};

				case BookItem::Column::Size:
					if (const auto size = item->GetData(index.column()).toULongLong())
						return Measure::GetSize(size);
					return {};

				default:
					return item->GetData(index.column());
			}

		case Qt::CheckStateRole:
			return m_checkable && index.column() == 0 ? item->GetCheckState() : QVariant{};

		case Role::Id:
			return item->GetId();

		case Role::Type:
			return QVariant::fromValue(item->GetType());

		case Role::MappedColumn:
			return item->RemapColumn(index.column());

		default:
			break;
	}

	return {};
}

bool BaseModel::setData(const QModelIndex & index, const QVariant & value, const int role)
{
	if (index.isValid())
	{
		auto * item = static_cast<DataItem *>(index.internalPointer());
		switch (role)
		{
			case Role::CheckState:
				return item->SetCheckState(value.value<Qt::CheckState>()), true;

			default:
				return assert(false && "unexpected role"), false;
		}
	}

	switch (role)
	{
		case Role::Checkable:
			m_checkable = value.toBool();
			return true;

		default:
			break;
	}

	return assert(false && "unexpected role"), false;
}

Qt::ItemFlags BaseModel::flags(const QModelIndex & index) const
{
	auto flags = QAbstractItemModel::flags(index);

	if (m_checkable && index.column() == 0)
		flags |= Qt::ItemIsUserCheckable;

	return flags;
}
