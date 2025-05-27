#include "TreeViewDelegateBooks.h"

#include <QApplication>
#include <QHeaderView>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTreeView>

#include "fnd/FindPair.h"
#include "fnd/IsOneOf.h"
#include "fnd/ValueGuard.h"
#include "fnd/observable.h"

#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/ui/IUiFactory.h"

#include "util/ISettings.h"

#include "Measure.h"
#include "log.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

QString PassThruDelegate(const QVariant& value)
{
	return value.toString();
}

QString NumberDelegate(const QVariant& value)
{
	bool ok = false;
	const auto result = value.toInt(&ok);
	return ok && result > 0 ? QString::number(result) : QString {};
}

QString SizeDelegate(const QVariant& value)
{
	bool ok = false;
	const auto result = value.toULongLong(&ok);
	return ok && result > 0 ? Measure::GetSize(result) : QString {};
}

constexpr std::pair<int, TreeViewDelegateBooks::TextDelegate> DELEGATES[] {
	{	  BookItem::Column::Size,   &SizeDelegate },
	{ BookItem::Column::SeqNumber, &NumberDelegate },
};

class IBookRenderer // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IBookRenderer() = default;
	virtual void Render(QPainter* painter, QStyleOptionViewItem& o, const QModelIndex& index) const = 0;
};

class BookRendererDefault final : virtual public IBookRenderer
{
public:
	explicit BookRendererDefault(const QStyledItemDelegate& impl)
		: m_impl { impl }
	{
	}

private: // IBookRenderer
	void Render(QPainter* painter, QStyleOptionViewItem& o, const QModelIndex& index) const override
	{
		m_impl.QStyledItemDelegate::paint(painter, o, index);
	}

private:
	const QStyledItemDelegate& m_impl;
};

class RateRendererStars final : virtual public IBookRenderer
{
public:
	RateRendererStars(const int role, const ISettings& settings)
		: m_role { role }
		, m_starSymbol { settings.Get(Constant::Settings::STAR_SYMBOL_KEY, Constant::Settings::STAR_SYMBOL_DEFAULT) }
	{
	}

private: // IRateRenderer
	void Render(QPainter* painter, QStyleOptionViewItem& o, const QModelIndex& index) const override
	{
		const auto rate = index.data(m_role).toInt();
		o.text = rate < 1 || rate > 5 ? QString {} : QString(rate, QChar(m_starSymbol));
		QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &o, painter, nullptr);
	}

private:
	const int m_role;
	const int m_starSymbol;
};

class RateRendererNumber final : virtual public IBookRenderer
{
public:
	explicit RateRendererNumber(const int role)
		: m_role { role }
	{
	}

private: // IRateRenderer
	void Render(QPainter* painter, QStyleOptionViewItem& o, const QModelIndex& index) const override
	{
		o.text = index.data(m_role).toString();
		o.displayAlignment = Qt::AlignRight;
		QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &o, painter, nullptr);
	}

private:
	const int m_role;
};

std::unique_ptr<const IBookRenderer> GetLibRateRenderer(const ISettings& settings)
{
	return settings.Get(Constant::Settings::STAR_VIEW_PRECISION, Constant::Settings::STAR_VIEW_PRECISION_DEFAULT) <= Constant::Settings::STAR_VIEW_PRECISION_DEFAULT
	         ? std::unique_ptr<const IBookRenderer> { std::make_unique<RateRendererStars>(Role::LibRate, settings) }
	         : std::unique_ptr<const IBookRenderer> { std::make_unique<RateRendererNumber>(Role::LibRate) };
}

} // namespace

class TreeViewDelegateBooks::Impl final
	: public QStyledItemDelegate
	, public Observable<ITreeViewDelegate::IObserver>
{
public:
	Impl(const IUiFactory& uiFactory, const ISettings& settings)
		: m_view { uiFactory.GetTreeView() }
		, m_textDelegate { &PassThruDelegate }
		, m_libRateRenderer { GetLibRateRenderer(settings) }
		, m_userRateRenderer { std::make_unique<RateRendererStars>(Role::UserRate, settings) }
	{
	}

private: // QStyledItemDelegate
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		auto o = option;
		if (index.data(Role::Type).value<ItemType>() == ItemType::Books)
			return RenderBooks(painter, o, index);

		if (index.column() != 0)
			return;

		const auto* header = m_view.header();
		int width = 0;
		for (auto i = 0, sz = header->count(); i < sz; ++i)
			width += header->sectionSize(i);
		width -= o.rect.x();

		o.rect.setWidth(width);
		QStyledItemDelegate::paint(painter, o, index);
	}

	QString displayText(const QVariant& value, const QLocale& /*locale*/) const override
	{
		return m_textDelegate(value);
	}

private:
	void RenderBooks(QPainter* painter, QStyleOptionViewItem& o, const QModelIndex& index) const
	{
		const auto column = BookItem::Remap(index.column());
		if (IsOneOf(column, BookItem::Column::Size, BookItem::Column::SeqNumber))
			o.displayAlignment = Qt::AlignRight;

		if (index.data(Role::IsRemoved).toBool())
			o.palette.setColor(QPalette::ColorRole::Text, Qt::gray);

		ValueGuard valueGuard(m_textDelegate, FindSecond(DELEGATES, column, &PassThruDelegate));
		const auto* renderer = FindSecond(m_rateRenderers, column, m_defaultRenderer.get());
		renderer->Render(painter, o, index);
	}

private:
	QTreeView& m_view;
	mutable TextDelegate m_textDelegate;
	std::unique_ptr<const IBookRenderer> m_defaultRenderer { std::make_unique<BookRendererDefault>(*this) };
	std::unique_ptr<const IBookRenderer> m_libRateRenderer;
	std::unique_ptr<const IBookRenderer> m_userRateRenderer;
	const std::vector<std::pair<int, const IBookRenderer*>> m_rateRenderers {
		{  BookItem::Column::LibRate,  m_libRateRenderer.get() },
		{ BookItem::Column::UserRate, m_userRateRenderer.get() },
	};
};

TreeViewDelegateBooks::TreeViewDelegateBooks(const std::shared_ptr<const IUiFactory>& uiFactory, const std::shared_ptr<const ISettings>& settings)
	: m_impl(*uiFactory, *settings)
{
	PLOGV << "TreeViewDelegateBooks created";
}

TreeViewDelegateBooks::~TreeViewDelegateBooks()
{
	PLOGV << "TreeViewDelegateBooks destroyed";
}

QAbstractItemDelegate* TreeViewDelegateBooks::GetDelegate() noexcept
{
	return m_impl.get();
}

void TreeViewDelegateBooks::OnModelChanged()
{
}

void TreeViewDelegateBooks::SetEnabled(bool /*enabled*/) noexcept
{
}

void TreeViewDelegateBooks::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void TreeViewDelegateBooks::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
