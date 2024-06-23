#include "TreeViewDelegateNavigation.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QStyledItemDelegate>
#include <QSvgWidget>
#include <QToolButton>

#include <plog/Log.h>

#include "fnd/observable.h"

#include "interface/ui/IUiFactory.h"

using namespace HomeCompa::Flibrary;

namespace {

QByteArray GetSvgWidgetContent(const QPalette & palette)
{
	QFile file(":/icons/plus.svg");
	[[maybe_unused]] const auto ok = file.open(QIODevice::ReadOnly);
	assert(ok);

	const auto getColor = [&] (const QPalette::ColorRole role)
	{
		const auto color = palette.color(role);
		return QString("#%1%2%3").arg(color.red(), 2, 16, QChar { '0' }).arg(color.green(), 2, 16, QChar { '0' }).arg(color.blue(), 2, 16, QChar { '0' });
	};

	return QString::fromUtf8(file.readAll()).arg(getColor(QPalette::Mid)).arg(getColor(QPalette::Base)).arg(45).toUtf8();
}

}

class TreeViewDelegateNavigation::Impl final
	: public QStyledItemDelegate
	, public Observable<ITreeViewDelegate::IObserver>
{
public:
	explicit Impl(const IUiFactory & uiFactory)
		: m_view(uiFactory.GetAbstractItemView())
		, m_svgWidgetContent(GetSvgWidgetContent(m_view.palette()))
	{
	}

	void OnModelChanged()
	{
		disconnect(m_connection);
		m_connection = connect(m_view.selectionModel(), &QItemSelectionModel::selectionChanged, this, &Impl::OnSelectionChanged);
	}

private: // QStyledItemDelegate
	QWidget * createEditor(QWidget * parent, [[maybe_unused]] const QStyleOptionViewItem & option, const QModelIndex & index) const override
	{
		auto * btn = new QToolButton(parent);
		btn->setAutoRaise(true);
		QPersistentModelIndex persistentIndex { index };
		connect(btn, &QAbstractButton::clicked, [this_ = const_cast<Impl*>(this), persistentIndex = std::move(persistentIndex)]
		{
			this_->Perform(&IObserver::OnButtonClicked, std::cref(static_cast<const QModelIndex&>(persistentIndex)));
		});

		auto * icon = new QSvgWidget(btn);
		icon->load(m_svgWidgetContent);
		auto * layout = new QHBoxLayout(btn);
		btn->setLayout(layout);
		btn->layout()->addWidget(icon);

		return btn;
	}

	void updateEditorGeometry(QWidget * editor, const QStyleOptionViewItem & option, [[maybe_unused]] const QModelIndex & index) const override
	{
		auto rect = option.rect;
		rect.setLeft(rect.right() - rect.height());

		const auto size = rect.height() / 10;
		editor->layout()->setContentsMargins(size, size, size, size);

		editor->setGeometry(rect);
	}

private:
	void OnSelectionChanged(const QItemSelection & selected, const QItemSelection & deselected) const
	{
		for (const auto index : deselected.indexes())
			m_view.closePersistentEditor(index);

		for (const auto index : selected.indexes())
			m_view.openPersistentEditor(index);
	}

private:
	QAbstractItemView & m_view;
	QMetaObject::Connection m_connection;
	QByteArray m_svgWidgetContent;
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

QAbstractItemDelegate * TreeViewDelegateNavigation::GetDelegate()
{
	return m_impl.get();
}

void TreeViewDelegateNavigation::OnModelChanged()
{
	m_impl->OnModelChanged();
}

void TreeViewDelegateNavigation::RegisterObserver(IObserver * observer)
{
	m_impl->Register(observer);
}

void TreeViewDelegateNavigation::UnregisterObserver(IObserver * observer)
{
	m_impl->Unregister(observer);
}
