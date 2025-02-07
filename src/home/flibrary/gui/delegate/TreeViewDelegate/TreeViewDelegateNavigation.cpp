#include "TreeViewDelegateNavigation.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QPainter>

#include <plog/Log.h>

#include "fnd/observable.h"
#include "interface/ui/IUiFactory.h"
#include "util/ColorUtil.h"

using namespace HomeCompa;
using namespace Flibrary;

class TreeViewDelegateNavigation::Impl final
	: public QStyledItemDelegate
	, public Observable<ITreeViewDelegate::IObserver>
{
public:
	explicit Impl(const IUiFactory & uiFactory)
		: m_view(uiFactory.GetAbstractItemView())
	{
	}

	void OnModelChanged()
	{
		disconnect(m_connection);
		if (m_enabled)
			m_connection = connect(m_view.selectionModel(), &QItemSelectionModel::selectionChanged, this, &Impl::OnSelectionChanged);
	}

	void SetEnabled(const bool enabled) noexcept
	{
		m_enabled = enabled;
	}

private: // QStyledItemDelegate
	QWidget * createEditor(QWidget * parent, const QStyleOptionViewItem & option, const QModelIndex & index) const override
	{
		if (!m_enabled)
			return QStyledItemDelegate::createEditor(parent, option, index);

		auto * btn = new QToolButton(parent);
		btn->setIcon(QIcon(":/icons/remove.svg"));
		btn->setAutoRaise(true);
		QPersistentModelIndex persistentIndex { index };
		connect(btn, &QAbstractButton::clicked, [this_ = const_cast<Impl*>(this), persistentIndex = std::move(persistentIndex)]
		{
			this_->Perform(&IObserver::OnButtonClicked, std::cref(static_cast<const QModelIndex&>(persistentIndex)));
		});

		return btn;
	}

	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		if (!(option.state & QStyle::State_Selected))
			return QStyledItemDelegate::paint(painter, option, index);

		const QWidget* widget = option.widget;
		assert(widget);
		QStyle* style = widget->style();
		assert(style);
		style->drawControl(QStyle::CE_ItemViewItem, &option, painter, widget);
		const int textHMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, widget) + 1;
		const int textVMargin = style->pixelMetric(QStyle::PM_FocusFrameVMargin, nullptr, widget) - 1;
		style->drawItemText(painter, option.rect.adjusted(textHMargin, textVMargin, -textHMargin, -textVMargin), Qt::AlignLeft, option.palette, option.state & QStyle::State_Enabled, index.data(Qt::DisplayRole).toString());
	}

	void updateEditorGeometry(QWidget * editor, const QStyleOptionViewItem & option, [[maybe_unused]] const QModelIndex & index) const override
	{
		auto rect = option.rect;
		rect.setLeft(rect.right() - rect.height());
		editor->setGeometry(rect);
	}

private:
	void OnSelectionChanged(const QItemSelection & selected, const QItemSelection & deselected) const
	{
		for (const auto& index : deselected.indexes())
			m_view.closePersistentEditor(index);

		for (const auto& index : selected.indexes())
			m_view.openPersistentEditor(index);
	}

private:
	QAbstractItemView & m_view;
	QMetaObject::Connection m_connection;
	bool m_enabled { false };
};

TreeViewDelegateNavigation::TreeViewDelegateNavigation(const std::shared_ptr<const IUiFactory> & uiFactory)
	: m_impl(*uiFactory)
{
	PLOGD << "TreeViewDelegateNavigation created";
}

TreeViewDelegateNavigation::~TreeViewDelegateNavigation()
{
	PLOGD << "TreeViewDelegateNavigation destroyed";
}

QAbstractItemDelegate * TreeViewDelegateNavigation::GetDelegate() noexcept
{
	return m_impl.get();
}

void TreeViewDelegateNavigation::OnModelChanged()
{
	m_impl->OnModelChanged();
}

void TreeViewDelegateNavigation::SetEnabled(const bool enabled) noexcept
{
	m_impl->SetEnabled(enabled);
}

void TreeViewDelegateNavigation::RegisterObserver(IObserver * observer)
{
	m_impl->Register(observer);
}

void TreeViewDelegateNavigation::UnregisterObserver(IObserver * observer)
{
	m_impl->Unregister(observer);
}
