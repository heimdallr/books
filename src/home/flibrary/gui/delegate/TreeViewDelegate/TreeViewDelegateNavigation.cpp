#include "TreeViewDelegateNavigation.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QToolButton>

#include "fnd/algorithm.h"
#include "fnd/observable.h"

#include "interface/constants/ModelRole.h"
#include "interface/ui/IUiFactory.h"

#include "util/ColorUtil.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

class TreeViewDelegateNavigation::Impl final
	: public QStyledItemDelegate
	, public Observable<ITreeViewDelegate::IObserver>
{
public:
	explicit Impl(const IUiFactory& uiFactory)
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
	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		if (!m_enabled)
			return QStyledItemDelegate::createEditor(parent, option, index);

		auto* btn = new QToolButton(parent);
		btn->setIcon(QIcon(":/icons/remove.svg"));
		btn->setAutoRaise(true);
		QPersistentModelIndex persistentIndex { index };
		connect(btn, &QAbstractButton::clicked, [this_ = const_cast<Impl*>(this), persistentIndex = std::move(persistentIndex)] {
			this_->Perform(&IObserver::OnButtonClicked, std::cref(static_cast<const QModelIndex&>(persistentIndex)));
		});

		return btn;
	}

	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		if (Util::Set(*m_height, option.rect.size().height()))
			Perform(&IObserver::OnLineHeightChanged, *m_height);

		auto o = option;
		if (index.data(Role::IsRemoved).toBool())
			o.palette.setColor(QPalette::ColorRole::Text, Qt::gray);

		if (!m_enabled || !(o.state & QStyle::State_Selected))
			return QStyledItemDelegate::paint(painter, o, index);

		const QWidget* widget = o.widget;
		assert(widget);
		QStyle* style = widget->style();
		assert(style);
		style->drawControl(QStyle::CE_ItemViewItem, &o, painter, widget);
		const int textHMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, widget) + 1;
		const int textVMargin = style->pixelMetric(QStyle::PM_FocusFrameVMargin, nullptr, widget) - 1;
		style->drawItemText(painter, o.rect.adjusted(textHMargin, textVMargin, -textHMargin, -textVMargin), Qt::AlignLeft, o.palette, o.state & QStyle::State_Enabled, index.data(Qt::DisplayRole).toString());
	}

	void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, [[maybe_unused]] const QModelIndex& index) const override
	{
		auto rect = option.rect;
		rect.setLeft(rect.right() - rect.height());
		editor->setGeometry(rect);
	}

private:
	void OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected) const
	{
		for (const auto& index : deselected.indexes())
			m_view.closePersistentEditor(index);

		for (const auto& index : selected.indexes())
			m_view.openPersistentEditor(index);
	}

private:
	QAbstractItemView&      m_view;
	QMetaObject::Connection m_connection;
	bool                    m_enabled { false };
	std::unique_ptr<int>    m_height { std::make_unique<int>() };
};

TreeViewDelegateNavigation::TreeViewDelegateNavigation(const std::shared_ptr<const IUiFactory>& uiFactory)
	: m_impl(*uiFactory)
{
	PLOGV << "TreeViewDelegateNavigation created";
}

TreeViewDelegateNavigation::~TreeViewDelegateNavigation()
{
	PLOGV << "TreeViewDelegateNavigation destroyed";
}

QAbstractItemDelegate* TreeViewDelegateNavigation::GetDelegate() noexcept
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

void TreeViewDelegateNavigation::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void TreeViewDelegateNavigation::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
