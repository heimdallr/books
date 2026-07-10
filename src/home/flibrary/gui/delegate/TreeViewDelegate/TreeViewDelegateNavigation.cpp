#include "TreeViewDelegateNavigation.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QToolButton>

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
		btn->setFocusPolicy(Qt::FocusPolicy::NoFocus);
		QPersistentModelIndex persistentIndex { index };
		connect(btn, &QAbstractButton::clicked, [this_ = const_cast<Impl*>(this), persistentIndex = std::move(persistentIndex)] {
			this_->Perform(&IObserver::OnButtonClicked, std::cref(static_cast<const QModelIndex&>(persistentIndex)));
		});

		return btn;
	}

	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		auto o = option;
		if (index.data(Role::IsRemoved).toBool())
			o.palette.setColor(QPalette::ColorRole::Text, Qt::gray);

		if (index.column() == 1)
		{
			o.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
			o.textElideMode    = Qt::TextElideMode::ElideNone;
		}
		else
		{
			o.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
		}

		QStyledItemDelegate::paint(painter, o, index);
	}

	void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, [[maybe_unused]] const QModelIndex& index) const override
	{
		auto rect = option.rect;
		rect.setLeft(rect.right() - rect.height());
		editor->setGeometry(rect);
	}

	QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		auto size = QStyledItemDelegate::sizeHint(option, index);
		size.setHeight(std::max(size.height(), option.fontMetrics.height() + 10));
		return size;
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

void TreeViewDelegateNavigation::OnModelChanged(const QAbstractItemModel&)
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
