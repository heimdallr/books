#include "TreeViewDelegate.h"

#include <QAbstractScrollArea>
#include <QPainter>

#include <plog/Log.h>

#include "fnd/FindPair.h"
#include "fnd/IsOneOf.h"
#include "fnd/ValueGuard.h"

#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"
#include "interface/ui/IRateStarsProvider.h"
#include "interface/ui/IUiFactory.h"

#include "Measure.h"

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

struct TreeViewDelegateBooks::Impl
{
	QWidget & view;
	TreeViewDelegateBooks & self;
	std::shared_ptr<const IRateStarsProvider> rateStarsProvider;
	mutable TextDelegate textDelegate;

	Impl(TreeViewDelegateBooks & self
		, std::shared_ptr<const IRateStarsProvider> rateStarsProvider
		, const IUiFactory & uiFactory
	)
		: view(uiFactory.GetAbstractScrollArea())
		, self(self)
		, rateStarsProvider(std::move(rateStarsProvider))
		, textDelegate(&PassThruDelegate)
	{
	}

	void RenderLibRate(QPainter * painter, const QStyleOptionViewItem & o, const QModelIndex & index) const
	{
		const auto rate = index.data(Role::LibRate).toInt();
		if (rate < 1 || rate > 5)
			return;

		const auto stars = rateStarsProvider->GetStars(rate);
		const auto margin = o.rect.height() - stars.height();
		const auto width = o.rect.width() - 2 * margin / 3;
		const auto pixmap = stars.width() <= width ? stars : stars.copy(0, 0, width, stars.height());
		painter->drawPixmap(o.rect.topLeft() + QPoint(margin / 2, 2 * margin / 3), pixmap);
	}

	void RenderBooks(QPainter * painter, QStyleOptionViewItem & o, const QModelIndex & index) const
	{
		const auto column = BookItem::Remap(index.column());
		if (IsOneOf(column, BookItem::Column::Size, BookItem::Column::SeqNumber))
			o.displayAlignment = Qt::AlignRight;

		if (column == BookItem::Column::LibRate)
			o.palette.setColor(QPalette::ColorRole::Text, Qt::transparent);
		else if (index.data(Role::IsRemoved).toBool())
			o.palette.setColor(QPalette::ColorRole::Text, Qt::gray);

		ValueGuard valueGuard(textDelegate, FindSecond(DELEGATES, column, &PassThruDelegate));
		self.QStyledItemDelegate::paint(painter, o, index);
		if (column == BookItem::Column::LibRate)
			RenderLibRate(painter, o, index);
	}
};

TreeViewDelegateBooks::TreeViewDelegateBooks(const std::shared_ptr<const IUiFactory> & uiFactory
	, std::shared_ptr<const IRateStarsProvider> rateStarsProvider
	, QObject * parent
)
	: QStyledItemDelegate(parent)
	, m_impl(*this, std::move(rateStarsProvider), *uiFactory)
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
		return m_impl->RenderBooks(painter, o, index);

	if (index.column() != 0)
		return;

	o.rect.setWidth(m_impl->view.width() - QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent) - o.rect.x());
	QStyledItemDelegate::paint(painter, o, index);
}

QString TreeViewDelegateBooks::displayText(const QVariant & value, const QLocale & /*locale*/) const
{
	return m_impl->textDelegate(value);
}
