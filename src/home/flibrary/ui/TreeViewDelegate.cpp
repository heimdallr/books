#include "TreeViewDelegate.h"

#include <QAbstractScrollArea>
#include <QPainter>

#include <plog/Log.h>

#include "fnd/FindPair.h"
#include "fnd/IsOneOf.h"
#include "fnd/ValueGuard.h"

#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"

#include "logic/data/DataItem.h"

#include "Measure.h"
#include "interface/ui/IUiFactory.h"

using namespace HomeCompa::Flibrary;

namespace {

QString PassThruDelegate(const QVariant & value)
{
	return value.toString();
}

QString NumberDelegate(const QVariant & value)
{
	bool ok = false;
	const auto result = value.toInt(&ok);
	return ok && result > 0 ? QString::number(result) : QString {};
}

QString SizeDelegate(const QVariant & value)
{
	bool ok = false;
	const auto result = value.toULongLong(&ok);
	return ok && result > 0 ? Measure::GetSize(result) : QString {};
}

constexpr std::pair<int, TreeViewDelegateBooks::TextDelegate> DELEGATES[]
{
	{BookItem::Column::Size, &SizeDelegate},
	{BookItem::Column::LibRate, &NumberDelegate},
	{BookItem::Column::SeqNumber, &NumberDelegate},
};

}

TreeViewDelegateBooks::TreeViewDelegateBooks(const std::shared_ptr<class IUiFactory> & uiFactory, QObject * parent)
	: QStyledItemDelegate(parent)
	, m_view(uiFactory->GetAbstractScrollArea())
	, m_textDelegate(&PassThruDelegate)
{
	PLOGD << "TreeViewDelegateBooks created";
}

TreeViewDelegateBooks::~TreeViewDelegateBooks()
{
	PLOGD << "TreeViewDelegateBooks destroyed";
}

void TreeViewDelegateBooks::paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const
{
	auto o = option;
	if (index.data(Role::Type).value<ItemType>() == ItemType::Books)
	{
		const auto column = BookItem::Remap(index.column());
		if (IsOneOf(column, BookItem::Column::Size, BookItem::Column::LibRate, BookItem::Column::SeqNumber))
			o.displayAlignment = Qt::AlignRight;

		if (index.data(Role::IsRemoved).toBool())
			o.palette.setColor(QPalette::ColorRole::Text, Qt::gray);

		ValueGuard valueGuard(m_textDelegate, FindSecond(DELEGATES, column, &PassThruDelegate));
		return QStyledItemDelegate::paint(painter, o, index);
	}

	if (index.column() != 0)
		return;

	o.rect.setWidth(m_view.width() - QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent) - o.rect.x());
	QStyledItemDelegate::paint(painter, o, index);
}

QString TreeViewDelegateBooks::displayText(const QVariant & value, const QLocale & /*locale*/) const
{
	return m_textDelegate(value);
}
