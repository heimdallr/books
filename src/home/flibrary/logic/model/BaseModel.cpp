#include "BaseModel.h"

#include "fnd/IsOneOf.h"
#include "fnd/ScopedCall.h"
#include "fnd/algorithm.h"

#include "interface/Localization.h"
#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"
#include "interface/logic/ILibRateProvider.h"
#include "interface/logic/IModelProvider.h"

#include "data/DataItem.h"
#include "data/Genre.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

template <typename F>
void Enumerate(const IDataItem& root, const F& f)
{
	f(root);
	for (size_t i = 0, sz = root.GetChildCount(); i < sz; ++i)
		Enumerate(*root.GetChild(i), f);
}

}

BaseModel::BaseModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent)
	: QAbstractItemModel(parent)
	, m_data { modelProvider->GetData() }
	, m_libRateProvider { modelProvider->GetLibRateProvider() }
{
}

BaseModel::~BaseModel() = default;

QVariant BaseModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
	if (orientation != Qt::Horizontal)
		return {};

	switch (role)
	{
		case Role::HeaderName:
			return m_data->GetData(section);

		case Qt::DisplayRole:
		case Role::HeaderTitle:
			return Loc::Tr(Loc::Ctx::BOOK, m_data->GetData(section).toUtf8().data());

		default:
			break;
	}

	return {};
}

QVariant BaseModel::data(const QModelIndex& index, const int role) const
{
	assert(index.isValid());

	const auto* item = GetInternalPointer(index);
	if (item->GetType() == ItemType::Books)
	{
		if (role == Role::LibRate)
			return m_libRateProvider->GetLibRateString(item->GetId().toLongLong(), item->GetRawData(BookItem::Column::LibRate));

		if (item->RemapColumn(index.column()) == BookItem::Column::LibRate)
		{
			switch (role)
			{
				case Qt::DisplayRole:
					return m_libRateProvider->GetLibRateString(item->GetId().toLongLong(), item->GetRawData(BookItem::Column::LibRate));

				case Qt::ForegroundRole:
					return m_libRateProvider->GetForegroundBrush(item->GetId().toLongLong(), item->GetRawData(BookItem::Column::LibRate));

				default:
					break;
			}
		}
	}

	switch (role)
	{
		case Qt::DisplayRole:
		case Qt::ToolTipRole:
			return GetValue(*item, item->RemapColumn(index.column()));

		case Qt::CheckStateRole:
		case Role::CheckState:
			return item->GetCheckState();

		case Role::Checkable:
			return m_checkable;

		case Role::Id:
			return item->GetId();

		case Role::Type:
			return QVariant::fromValue(item->GetType());

		case Role::IsTree:
			return false;

		case Role::Flags:
			return QVariant::fromValue(item->GetFlags());

		case Role::IsRemoved:
			return item->IsRemoved();

		case Role::ChildCount:
			return item->GetChildCount();

		case Role::Remap:
			return item->RemapColumn(index.column());

#define BOOKS_COLUMN_ITEM(NAME) \
	case Role::NAME:            \
		return GetValue(*item, BookItem::Column::NAME);
			BOOKS_COLUMN_ITEMS_X_MACRO
#undef BOOKS_COLUMN_ITEM

		default:
			break;
	}

	return {};
}

bool BaseModel::setData(const QModelIndex& index, const QVariant& value, const int role)
{
	if (index.isValid())
	{
		auto* item = GetInternalPointer(index);
		switch (role)
		{
			case Role::CheckState:
				return item->SetCheckState(value.value<Qt::CheckState>()), true;

			case Qt::EditRole:
				return true;

			case Role::Flags:
				return item->SetFlags(value.value<IDataItem::Flags>()), true;

			default:
				break;
		}

		return assert(false && "unexpected role"), false;
	}

	switch (role)
	{
		case Role::Checkable:
			return Util::Set(m_checkable, value.toBool());

		case Role::Check:
			return Check(value, Qt::Checked);

		case Role::Uncheck:
			return Check(value, Qt::Unchecked);

		default:
			break;
	}

	return assert(false && "unexpected role"), false;
}

bool BaseModel::removeRows(const int row, const int count, const QModelIndex& parent)
{
	auto* parentItem = parent.isValid() ? GetInternalPointer(parent) : m_data.get();
	assert(parentItem);
	const auto uRow   = static_cast<size_t>(row);
	const auto uCount = static_cast<size_t>(count);
	if (uRow + uCount > parentItem->GetChildCount())
		return false;

	const ScopedCall removeGuard(
		[&] {
			beginRemoveRows(parent, row, row + count - 1);
		},
		[this]() {
			endRemoveRows();
		}
	);
	parentItem->RemoveChild(uRow, uCount);
	return true;
}

Qt::ItemFlags BaseModel::flags(const QModelIndex& index) const
{
	auto flags = QAbstractItemModel::flags(index);

	if (m_checkable && index.column() == 0)
		flags |= Qt::ItemIsUserCheckable;

	return flags;
}

IDataItem* BaseModel::GetInternalPointer(const QModelIndex& index) const
{
	return static_cast<IDataItem*>(index.internalPointer());
}

QVariant BaseModel::GetValue(const IDataItem& item, const int column)
{
	if (item.GetType() == ItemType::Books && IsOneOf(column, BookItem::Column::SeqNumber, BookItem::Column::Size))
	{
		bool       ok     = false;
		const auto result = item.GetRawData(column).toLongLong(&ok);
		return ok ? QVariant { result } : QVariant {};
	}

	return item.GetRawData(column);
}
