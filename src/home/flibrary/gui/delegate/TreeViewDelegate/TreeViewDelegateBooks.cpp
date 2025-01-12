#include "TreeViewDelegateBooks.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QPainter>
#include <QStyledItemDelegate>

#include <plog/Log.h>

#include "fnd/FindPair.h"
#include "fnd/IsOneOf.h"
#include "fnd/observable.h"
#include "fnd/ValueGuard.h"

#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"
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
	{ BookItem::Column::Size     , &SizeDelegate },
	{ BookItem::Column::LibRate  , &NumberDelegate },
	{ BookItem::Column::SeqNumber, &NumberDelegate },
};

}

class TreeViewDelegateBooks::Impl final
	: public QStyledItemDelegate
	, public Observable<ITreeViewDelegate::IObserver>
{
public:
	explicit Impl(const IUiFactory & uiFactory)
		: m_view { uiFactory.GetAbstractScrollArea() }
		, m_textDelegate { &PassThruDelegate }
	{
	}

private: // QStyledItemDelegate
	void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const override
	{
		auto o = option;
		if (index.data(Role::Type).value<ItemType>() == ItemType::Books)
			return RenderBooks(painter, o, index);

		if (index.column() != 0)
			return;

		o.rect.setWidth(m_view.width() - QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent) - o.rect.x());
		QStyledItemDelegate::paint(painter, o, index);
	}

	QString displayText(const QVariant & value, const QLocale & /*locale*/) const override
	{
		return m_textDelegate(value);
	}

private:
	void RenderBooks(QPainter * painter, QStyleOptionViewItem & o, const QModelIndex & index) const
	{
		const auto column = BookItem::Remap(index.column());
		if (IsOneOf(column, BookItem::Column::Size, BookItem::Column::SeqNumber))
			o.displayAlignment = Qt::AlignRight;

		if (index.data(Role::IsRemoved).toBool())
			o.palette.setColor(QPalette::ColorRole::Text, Qt::gray);

		ValueGuard valueGuard(m_textDelegate, FindSecond(DELEGATES, column, &PassThruDelegate));
		const auto isRate = IsOneOf(column, BookItem::Column::LibRate, BookItem::Column::UserRate);
		if (!isRate)
			return QStyledItemDelegate::paint(painter, o, index);

		static constexpr std::pair<int, int> columnToRole[]
		{
			{ BookItem::Column::LibRate , Role::LibRate },
			{ BookItem::Column::UserRate, Role::UserRate },
		};
		const auto rate = index.data(FindSecond(columnToRole, BookItem::Remap(index.column()))).toInt();

		if (rate < 1 || rate > 5)
			o.text.clear();
		else
			o.text.assign(rate, QChar(0x2B50));

		QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &o, painter, nullptr);
	}

private:
	QWidget & m_view;
	mutable TextDelegate m_textDelegate;

};

TreeViewDelegateBooks::TreeViewDelegateBooks(const std::shared_ptr<const IUiFactory> & uiFactory)
	: m_impl(*uiFactory)
{
	PLOGD << "TreeViewDelegateBooks created";
}

TreeViewDelegateBooks::~TreeViewDelegateBooks()
{
	PLOGD << "TreeViewDelegateBooks destroyed";
}

QAbstractItemDelegate * TreeViewDelegateBooks::GetDelegate() noexcept
{
	return m_impl.get();
}

void TreeViewDelegateBooks::OnModelChanged()
{
}

void TreeViewDelegateBooks::SetEnabled(bool /*enabled*/) noexcept
{
}

void TreeViewDelegateBooks::RegisterObserver(IObserver * observer)
{
	m_impl->Register(observer);
}

void TreeViewDelegateBooks::UnregisterObserver(IObserver * observer)
{
	m_impl->Unregister(observer);
}
