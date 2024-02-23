#include "BaseModel.h"

#include <QIcon>

#include "fnd/IsOneOf.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"
#include "interface/logic/IModelProvider.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

QVariant GetValue(const IDataItem & item, const int column)
{
	if (item.GetType() == ItemType::Books && IsOneOf(column, BookItem::Column::SeqNumber, BookItem::Column::Size))
	{
		bool ok = false;
		const auto result = item.GetRawData(column).toLongLong(&ok);
		return ok ? result : -1;
	}

	return item.GetRawData(column);
}

}

BaseModel::BaseModel(const std::shared_ptr<IModelProvider> & modelProvider, QObject * parent)
	: QAbstractItemModel(parent)
	, m_data(modelProvider->GetData())
{
}

BaseModel::~BaseModel() = default;

QVariant BaseModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
	if (orientation != Qt::Horizontal)
		return {};

	const auto isLangSection = m_data->GetType() == ItemType::Books && BookItem::Remap(section) == BookItem::Column::Lang;

	switch (role)
	{
		case Qt::DisplayRole:
			return isLangSection ? QVariant{} : Loc::Tr(Loc::Ctx::BOOK, m_data->GetData(section).toUtf8().data());

		case Qt::DecorationRole:
			return isLangSection ? QIcon(":/icons/language.png") : QVariant {};

		default:
			break;
	}

	return {};
}

QVariant BaseModel::data(const QModelIndex & index, const int role) const
{
	if (!index.isValid())
		return {};

	const auto * item = static_cast<IDataItem *>(index.internalPointer());
	switch (role)
	{
		case Qt::DisplayRole:
		case Qt::ToolTipRole:
			return GetValue(*item, item->RemapColumn(index.column()));

		case Qt::CheckStateRole:
		case Role::CheckState:
			return m_checkable && index.column() == 0 ? item->GetCheckState() : QVariant{};

		case Role::Id:
			return item->GetId();

		case Role::Type:
			return QVariant::fromValue(item->GetType());

		case Role::IsTree:
			return false;

		case Role::IsRemoved:
			return item->IsRemoved();

#define	BOOKS_COLUMN_ITEM(NAME) case Role::NAME: return GetValue(*item, BookItem::Column::NAME);
		BOOKS_COLUMN_ITEMS_X_MACRO
#undef	BOOKS_COLUMN_ITEM

		default:
			break;
	}

	return {};
}

bool BaseModel::setData(const QModelIndex & index, const QVariant & value, const int role)
{
	if (index.isValid())
	{
		auto * item = static_cast<IDataItem *>(index.internalPointer());
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
