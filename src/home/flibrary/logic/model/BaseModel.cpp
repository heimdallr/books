#include "BaseModel.h"

#include <QBrush>
#include <QFont>

#include "fnd/IsOneOf.h"
#include "fnd/ScopedCall.h"
#include "fnd/algorithm.h"

#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"
#include "interface/localization.h"
#include "interface/logic/ILibRateProvider.h"
#include "interface/logic/IModelProvider.h"

#include "data/DataItem.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

QColor   REMOVED_ITEM_FOREGROUND_COLOR = Qt::gray;
QVariant REMOVED_ITEM_BACKGROUND_COLOR;
QFont    REMOVED_ITEM_FONT = [] {
	QFont font;
	font.setItalic(true);
	return font;
}();

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
	else if (item->GetType() == ItemType::Navigation)
	{
		if (role == Role::NavigationMode)
		{
			const auto navigationItem = item->To<NavigationItem>();
			return QVariant::fromValue(navigationItem ? navigationItem->GetNavigationMode() : NavigationMode::Unknown);
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

		case Qt::ForegroundRole:
			return item->IsRemoved() ? QBrush(REMOVED_ITEM_FOREGROUND_COLOR) : QVariant {};

		case Qt::BackgroundRole:
			return item->IsRemoved() ? REMOVED_ITEM_BACKGROUND_COLOR : QVariant {};

		case Qt::FontRole:
			return item->IsRemoved() ? REMOVED_ITEM_FONT : QVariant {};

		case Role::CheckableColumn:
			return m_checkableColumn ? QVariant::fromValue(*m_checkableColumn) : QVariant {};

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
			return static_cast<qulonglong>(item->GetChildCount());

		case Role::Remap:
			return item->RemapColumn(index.column());

#define BOOKS_COLUMN_ITEM(NAME)  case Role::NAME: return GetValue(*item, BookItem::Column::NAME);
			BOOKS_COLUMN_ITEMS_X_MACRO
#undef BOOKS_COLUMN_ITEM

		default:
			break;
	}

	return {};
}

template <typename T>
using FontPropertySetter = void (QFont::*)(T);
template <typename T>
void SetFontProperty(QFont& font, const QVariant& value, FontPropertySetter<T>) = delete;

template <>
void SetFontProperty<bool>(QFont& font, const QVariant& value, const FontPropertySetter<bool> f)
{
	std::invoke(f, std::ref(font), value.toBool());
}

template <>
void SetFontProperty<int>(QFont& font, const QVariant& value, const FontPropertySetter<int> f)
{
	bool ok = false;
	if (const auto v = value.toInt(&ok); ok)
		std::invoke(f, std::ref(font), v);
}

template <>
void SetFontProperty<qreal>(QFont& font, const QVariant& value, const FontPropertySetter<qreal> f)
{
	bool ok = false;
	if (const auto v = value.toInt(&ok); ok)
		std::invoke(f, std::ref(font), v);
}

template <typename T>
concept EnumType = std::is_enum_v<T>;

template <EnumType T>
void SetFontProperty(QFont& font, const QVariant& value, const FontPropertySetter<T> f)
{
	bool ok = false;
	if (const auto v = value.toInt(&ok); ok)
		std::invoke(f, std::ref(font), static_cast<T>(v));
}

template <>
void SetFontProperty<const QString&>(QFont& font, const QVariant& value, const FontPropertySetter<const QString&> f)
{
	std::invoke(f, std::ref(font), value.toString());
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

#define BOOKS_COLUMN_ITEM(NAME) case Role::NAME: item->SetData(value.toString(), role - Role::FirstItemColumn); return true;
				BOOKS_COLUMN_ITEMS_X_MACRO
#undef BOOKS_COLUMN_ITEM

			default:
				break;
		}

		return assert(false && "unexpected role"), false;
	}

	switch (role)
	{
		case Role::CheckableColumn:
			if (!value.isValid() || ((m_checkableColumn = value.toInt()), m_checkableColumn < 0))
				m_checkableColumn.reset();

			return true;

		case Role::Check:
			return Check(value, Qt::Checked);

		case Role::Uncheck:
			return Check(value, Qt::Unchecked);

		case Role::ColumnCount:
			return Util::Set(m_columnCount, value.toInt());

		case Role::SetForIds:
			return m_data && m_data->Enumerate([args = value.value<SetForIdsArg>()](IDataItem& item) {
				if (args.ids.contains(item.GetId().toLongLong()))
					item.SetData(args.value, args.column);
				return true;
			});

		case Role::RemovedItemBackgroundColor:
			return Util::Set(REMOVED_ITEM_BACKGROUND_COLOR, QVariant::fromValue(QBrush(value.value<QColor>())));

		case Role::RemovedItemForegroundColor:
			return Util::Set(REMOVED_ITEM_FOREGROUND_COLOR, value.value<QColor>());

#define FONT_SETTING_SETTER_ITEM(NAME) case Role::RemovedItemFont_##NAME: return SetFontProperty(REMOVED_ITEM_FONT, value, &QFont::NAME), true;
			FONT_SETTING_SETTER_ITEMS_X_MACRO
#undef FONT_SETTING_SETTER_ITEM

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

	if (m_checkableColumn && index.column() == *m_checkableColumn)
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
