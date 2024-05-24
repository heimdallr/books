#include "TreeViewDelegate.h"

#include <QAbstractScrollArea>
#include <QPainter>
#include <QSvgRenderer>

#include <plog/Log.h>

#include "fnd/FindPair.h"
#include "fnd/IsOneOf.h"
#include "fnd/ValueGuard.h"

#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"

#include "Measure.h"
#include "interface/ui/IUiFactory.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto TRANSPARENT = "transparent";
constexpr auto COLOR = "#CCCC00";

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
	mutable TextDelegate textDelegate;
	mutable QPixmap stars[5];
	mutable QString starsFileContent;

	Impl(TreeViewDelegateBooks & self, const IUiFactory & uiFactory)
		: view(uiFactory.GetAbstractScrollArea())
		, self(self)
		, textDelegate(&PassThruDelegate)
	{
	}

	void RenderLibRate(QPainter * painter, const QStyleOptionViewItem & o, const QModelIndex & index) const
	{
		const auto rate = index.data(Role::LibRate).toInt();
		if (rate < 1 || rate > 5)
			return;

		const auto height = 3 * o.rect.height() / 5;

		auto & star = stars[rate - 1];
		if (star.isNull() || star.height() != height)
		{
			if (starsFileContent.isEmpty())
			{
				QFile file(":/icons/stars.svg");
				[[maybe_unused]] const auto ok = file.open(QIODevice::ReadOnly);
				assert(ok);
				starsFileContent = QString::fromUtf8(file.readAll());
			}
			const char * colors[5];
			for (int i = 0; i < rate; ++i)
				colors[i] = COLOR;
			for (int i = rate; i < 5; ++i)
				colors[i] = TRANSPARENT;
			const auto content = starsFileContent.arg(colors[0]).arg(colors[1]).arg(colors[2]).arg(colors[3]).arg(colors[4]);
			QSvgRenderer renderer(content.toUtf8());
			assert(renderer.isValid());

			const auto defaultSize = renderer.defaultSize();
			star = QPixmap(height * defaultSize.width() / defaultSize.height(), height);
			star.fill(Qt::transparent);
			QPainter p(&star);
			renderer.render(&p, QRect(QPoint{}, star.size()));
		}

		const auto margin = o.rect.height() - height;
		const auto width = o.rect.width() - 2 * margin / 3;
		const auto pixmap = star.width() <= width ? star : star.copy(0, 0, width, height);
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

TreeViewDelegateBooks::TreeViewDelegateBooks(const std::shared_ptr<const IUiFactory> & uiFactory, QObject * parent)
	: QStyledItemDelegate(parent)
	, m_impl(*this, *uiFactory)
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
