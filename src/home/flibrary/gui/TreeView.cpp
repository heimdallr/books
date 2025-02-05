#include "ui_TreeView.h"
#include "TreeView.h"

#include <ranges>
#include <stack>

#include <QActionGroup>
#include <QMenu>
#include <QPainter>
#include <QResizeEvent>
#include <QSvgRenderer>
#include <QSvgWidget>
#include <QTimer>

#include <plog/Log.h>

#include "fnd/FindPair.h"
#include "fnd/IsOneOf.h"
#include "fnd/ScopedCall.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/ITreeViewController.h"
#include "interface/ui/ITreeViewDelegate.h"
#include "interface/ui/IUiFactory.h"

#include "util/ColorUtil.h"
#include "util/ISettings.h"

#include "ItemViewToolTipper.h"
#include "ModeComboBox.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto VALUE_MODE_KEY = "ui/%1/ValueMode";
constexpr auto COLUMN_WIDTH_LOCAL_KEY = "%1/Width";
constexpr auto COLUMN_INDEX_LOCAL_KEY = "%1/Index";
constexpr auto COLUMN_HIDDEN_LOCAL_KEY = "%1/Hidden";
constexpr auto SORT_INDICATOR_COLUMN_KEY = "Sort/Index";
constexpr auto SORT_INDICATOR_ORDER_KEY = "Sort/Order";
constexpr auto RECENT_LANG_FILTER_KEY = "ui/language";

class HeaderView final
	: public QHeaderView
{
public:
	explicit HeaderView(QWidget * parent = nullptr)
		: QHeaderView(Qt::Horizontal, parent)
	{
		setFirstSectionMovable(false);
		setSectionsMovable(true);
		setSortIndicator(0, Qt::AscendingOrder);
	}

private: // QHeaderView
	void paintSection(QPainter * painter, const QRect & rect, const int logicalIndex) const override
	{
		if (!model())
			return QHeaderView::paintSection(painter, rect, logicalIndex);

		const ScopedCall painterGuard([=] { painter->save(); }, [=] { painter->restore(); });

		const auto palette = QApplication::palette();

		painter->setPen(QPen(palette.color(QPalette::Dark), 2));
		painter->fillRect(rect, palette.color(QPalette::Base));
		painter->drawRect(rect);

		if (!PaintIcon(painter, rect, logicalIndex))
			PaintText(painter, rect, logicalIndex);

		if (logicalIndex != sortIndicatorSection())
			return;

		painter->setPen(palette.color(QPalette::Text));
		painter->setBrush(palette.color(QPalette::Text));

		const auto size = rect.height() / 4.0;
		const auto height = std::sqrt(2.0) * size / 2;
		auto triangle = QPolygonF({ QPointF{0.0, height}, QPointF{size, height}, QPointF{size / 2, 0}, QPointF{0.0, height} });
		if (sortIndicatorOrder() == Qt::DescendingOrder)
			triangle = QTransform(1, 0, 0, -1, 0, height).map(triangle);

		painter->drawPolygon(triangle.translated(rect.right() - 4 * size / 3, size / 2));
	}

private:
	bool PaintIcon(QPainter * painter, const QRect & rect, const int logicalIndex) const
	{
		const auto icon = model()->headerData(logicalIndex, orientation(), Qt::DecorationRole);
		if (!icon.isValid())
			return false;

		if (!m_svgRenderer.isValid())
		{
			QFile file(icon.toString());
			if (!file.open(QIODevice::ReadOnly))
				return false;

			const auto content = QString::fromUtf8(file.readAll()).arg(Util::ToString(palette(), QPalette::Text));
			if (!m_svgRenderer.load(content.toUtf8()))
				return false;
		}

		auto iconSize = m_svgRenderer.defaultSize();
		const auto size = 6 * std::min(rect.width(), rect.height()) / 10;
		iconSize = iconSize.width() > iconSize.height() ? QSize { size, size * iconSize.height() / iconSize.width() } : QSize { size * iconSize.width() / iconSize.height(), size };
		const auto topLeft = rect.topLeft() + QPoint { (rect.width() - size) / 2, 2 * (rect.height() - size) / 3 };
		m_svgRenderer.render(painter, QRect { topLeft, iconSize });

		return true;
	}

	void PaintText(QPainter * painter, const QRect & rect, const int logicalIndex) const
	{
		const auto text = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();
		painter->setPen(QApplication::palette().color(QPalette::Text));
		painter->drawText(rect, Qt::AlignCenter, text);
	}

private:
	mutable QSvgRenderer m_svgRenderer;
};

void TreeOperation(const QAbstractItemModel & model, const QModelIndex & index, const std::function<void(const QModelIndex &)> & f)
{
	f(index);
	for (int row = 0, sz = model.rowCount(index); row < sz; ++row)
		TreeOperation(model, model.index(row, 0, index), f);
}

}

class TreeView::Impl final
	: ITreeViewController::IObserver
	, ITreeViewDelegate::IObserver
	, ModeComboBox::IValueApplier
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(TreeView & self
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<IUiFactory> uiFactory
		, std::shared_ptr<ItemViewToolTipper> itemViewToolTipper
		, const std::shared_ptr<ICollectionController> & collectionController
	)
		: m_self(self)
		, m_controller(uiFactory->GetTreeViewController())
		, m_settings(std::move(settings))
		, m_uiFactory(std::move(uiFactory))
		, m_itemViewToolTipper(std::move(itemViewToolTipper))
		, m_delegate(std::shared_ptr<ITreeViewDelegate>())
		, m_currentCollectionId(collectionController->GetActiveCollectionId())
	{
		Setup();
	}

	~Impl() override
	{
		SaveHeaderLayout();
		m_controller->UnregisterObserver(this);
		m_delegate->UnregisterObserver(this);
	}

	void SetNavigationModeName(QString navigationModeName)
	{
		SaveHeaderLayout();
		m_navigationModeName = std::move(navigationModeName);
	}

	void ShowRemoved(const bool showRemoved)
	{
		m_showRemoved = showRemoved;
		if (auto * model = m_ui.treeView->model())
		{
			model->setData({}, m_showRemoved, Role::ShowRemovedFilter);
			OnCountChanged();
			Find(m_currentId, Role::Id);
		}
	}

	QAbstractItemView * GetView() const
	{
		return m_ui.treeView;
	}

	void FillContextMenu(QMenu & menu)
	{
		m_controller->RequestContextMenu(m_ui.treeView->currentIndex(), GetContextMenuOptions(), [&] (const QString & id, const IDataItem::Ptr & item)
		{
			if (m_ui.treeView->currentIndex().data(Role::Id).toString() == id)
				GenerateMenu(menu, *item);
		});
	}

	void ResizeEvent(const QResizeEvent * event)
	{
		auto size = m_ui.cbMode->height();
		m_ui.btnNew->setMinimumSize(size, size);
		m_ui.btnNew->setMaximumSize(size, size);
		m_ui.cbValueMode->setMinimumSize(size, size);
		m_ui.cbValueMode->setMaximumSize(size, size);
		size /= 10;
		m_ui.btnNew->layout()->setContentsMargins(size, size, size, size);

		if (m_controller->GetItemType() != ItemType::Books || m_recentMode.isEmpty() || m_navigationModeName.isEmpty())
			return;

		m_ui.treeView->header()->resizeSection(0, m_ui.treeView->header()->sectionSize(0) + (event->size().width() - event->oldSize().width()));
	}

private: // ITreeViewController::IObserver
	void OnModeChanged(const int index) override
	{
		m_ui.value->setText({});
		m_ui.cbMode->setCurrentIndex(index);
	}

	void OnModelChanged(QAbstractItemModel * model) override
	{
		if (m_ui.treeView->model() != model)
			OnModelChangedImpl(model);

		RestoreHeaderLayout();
		OnCountChanged();
		OnValueChanged();
	}

	void OnContextMenuTriggered(const QString & /*id*/, const IDataItem::Ptr & item) override
	{
		switch (static_cast<BooksMenuAction>(item->GetData(MenuItem::Column::Id).toInt()))  // NOLINT(clang-diagnostic-switch-enum)
		{
			case BooksMenuAction::Collapse:
				for (const auto & row : m_ui.treeView->selectionModel()->selectedRows())
					TreeOperation(*m_ui.treeView->model(), row, [this] (const auto & index) { m_ui.treeView->collapse(index); });
				return;
			case BooksMenuAction::Expand:
				for (const auto & row : m_ui.treeView->selectionModel()->selectedRows())
					TreeOperation(*m_ui.treeView->model(), row, [this] (const auto & index) { m_ui.treeView->expand(index); });
				return;
			case BooksMenuAction::CollapseAll:
				return m_ui.treeView->collapseAll();
			case BooksMenuAction::ExpandAll:
				return m_ui.treeView->expandAll();
			default:
				break;
		}

		if (true
			&& IsOneOf(static_cast<BooksMenuAction>(item->GetData(MenuItem::Column::Id).toInt()), BooksMenuAction::SendAsArchive, BooksMenuAction::SendAsIs, BooksMenuAction::SendAsScript)
			&& item->GetData(MenuItem::Column::HasError).toInt()
			)
			m_uiFactory->ShowWarning(Loc::Tr(Loc::Ctx::ERROR, Loc::BOOKS_EXTRACT_ERROR));
	}

private: // ITreeViewDelegate::IObserver
	void OnButtonClicked(const QModelIndex&) override
	{
		assert(m_removeItems);
		m_removeItems(m_ui.treeView->selectionModel()->selectedIndexes());
	}

private: //	ModeComboBox::IValueApplier
	void Find() override
	{
		if (!m_ui.value->text().isEmpty())
			return Find(m_ui.value->text(), m_controller->GetItemType() == ItemType::Books ? static_cast<int>(Role::Title) : Qt::DisplayRole);

		if (!m_currentId.isEmpty())
			return Find(m_currentId, Role::Id);

		m_ui.treeView->setCurrentIndex(m_ui.treeView->model()->index(0, 0));
	}

	void Filter() override
	{
		m_filterTimer.start();
	}

private:
	void OnModelChangedImpl(QAbstractItemModel * model)
	{
		m_ui.treeView->setModel(model);
		m_ui.treeView->setRootIsDecorated(m_controller->GetViewMode() == ViewMode::Tree);

		connect(m_ui.treeView->selectionModel(), &QItemSelectionModel::currentRowChanged, &m_self, [&] (const QModelIndex & index)
		{
			m_controller->SetCurrentId(m_currentId = index.data(Role::Id).toString());
		});

		if (m_controller->GetItemType() == ItemType::Books)
		{
			m_languageContextMenu.reset();
			model->setData({}, true, Role::Checkable);
			model->setData({}, m_showRemoved, Role::ShowRemovedFilter);
			SetLanguageFilter();
		}
		else
		{
			m_delegate->SetEnabled(static_cast<bool>((m_removeItems = m_controller->GetRemoveItems())));
		}

		m_delegate->OnModelChanged();

		if (auto newItemCreator = m_controller->GetNewItemCreator(); !newItemCreator)
		{
			m_ui.btnNew->setVisible(false);
		}
		else
		{
			m_ui.btnNew->setVisible(true);
			m_ui.btnNew->disconnect();
			connect(m_ui.btnNew, &QAbstractButton::clicked, &m_self, std::move(newItemCreator));
		}

		if (model->rowCount() == 0)
			m_controller->SetCurrentId({});
	}

	ITreeViewController::RequestContextMenuOptions GetContextMenuOptions() const
	{
		static constexpr auto hasCollapsedExpanded = ITreeViewController::RequestContextMenuOptions::HasExpanded | ITreeViewController::RequestContextMenuOptions::HasCollapsed;

		const auto & model = *m_ui.treeView->model();

		ITreeViewController::RequestContextMenuOptions options
			= (model.data({}, Role::IsTree).toBool()           ? ITreeViewController::RequestContextMenuOptions::IsTree       : ITreeViewController::RequestContextMenuOptions::None)
			| (m_ui.treeView->selectionModel()->hasSelection() ? ITreeViewController::RequestContextMenuOptions::HasSelection : ITreeViewController::RequestContextMenuOptions::None)
			;

		if (!!(options & ITreeViewController::RequestContextMenuOptions::IsTree))
		{
			const auto checkIndex = [&] (const QModelIndex & index, const ITreeViewController::RequestContextMenuOptions onExpanded, const ITreeViewController::RequestContextMenuOptions onCollapsed)
			{
				assert(index.isValid());
				const auto result = model.rowCount(index) > 0;
				if (result)
					options |= m_ui.treeView->isExpanded(index) ? onExpanded : onCollapsed;
				return result;
			};

			if (m_ui.treeView->currentIndex().isValid())
				checkIndex(m_ui.treeView->currentIndex(), ITreeViewController::RequestContextMenuOptions::NodeExpanded, ITreeViewController::RequestContextMenuOptions::NodeCollapsed);

			std::stack<QModelIndex> stack { {QModelIndex{}} };
			while (!stack.empty())
			{
				const auto parent = stack.top();
				stack.pop();

				if ((options & hasCollapsedExpanded) == hasCollapsedExpanded)
					break;

				for (int i = 0, sz = model.rowCount(parent); i < sz; ++i)
				{
					const auto index = model.index(i, 0, parent);
					if (checkIndex(index, ITreeViewController::RequestContextMenuOptions::HasExpanded, ITreeViewController::RequestContextMenuOptions::HasCollapsed))
						stack.push(index);
				}
			}
		}

		return options;
	}

	void OnContextMenuReady(const QString & id, const IDataItem::Ptr & item)
	{
		if (m_ui.treeView->currentIndex().data(Role::Id).toString() != id)
			return;

		QMenu menu;
		menu.setFont(m_self.font());
		GenerateMenu(menu, *item);
		if (!menu.isEmpty())
			menu.exec(QCursor::pos());
	}

	void GenerateMenu(QMenu & menu, const IDataItem & item)
	{
		const auto font = menu.font();
		const QFontMetrics metrics(font);
		std::stack<std::pair<const IDataItem *, QMenu *>> stack { {{&item, &menu}} };
		while (!stack.empty())
		{
			auto [parent, subMenu] = stack.top();
			stack.pop();

			auto maxWidth = subMenu->minimumWidth();

			for (size_t i = 0, sz = parent->GetChildCount(); i < sz; ++i)
			{
				auto child = parent->GetChild(i);
				const auto enabledStr = child->GetData(MenuItem::Column::Enabled);
				const auto enabled = enabledStr.isEmpty() || QVariant(enabledStr).toBool();
				const auto title = child->GetData().toStdString();
				const auto titleText = Loc::Tr(m_controller->TrContext(), title.data());
				auto statusTip = titleText;
				statusTip.replace("&", "");
				maxWidth = std::max(maxWidth, metrics.boundingRect(statusTip).width() + 3 * (metrics.ascent() + metrics.descent()));

				if (child->GetChildCount() != 0)
				{
					auto * subSubMenu = stack.emplace(child.get(), subMenu->addMenu(titleText)).second;
					subSubMenu->setFont(font);
					subSubMenu->setEnabled(enabled);
					subSubMenu->setStatusTip(statusTip);
					continue;
				}

				if (const auto childId = child->GetData(MenuItem::Column::Id).toInt(); childId < 0)
				{
					subMenu->addSeparator();
					continue;
				}

				auto * action = subMenu->addAction(titleText, [&, child = std::move(child)] () mutable
				{
					const auto & view = *m_ui.treeView;
					m_settings->Set(GetRecentIdKey(), m_currentId = view.currentIndex().data(Role::Id).toString());

					auto selected = view.selectionModel()->selectedIndexes();
					const auto [begin, end] = std::ranges::remove_if(selected, [] (const auto & index)
					{
						return index.column() != 0;
					});
					selected.erase(begin, end);
					m_controller->OnContextMenuTriggered(view.model(), view.currentIndex(), selected, std::move(child));
				});
				action->setStatusTip(statusTip);

				action->setEnabled(enabled);
			}

			subMenu->setMinimumWidth(maxWidth);
		}
	}

	void Setup()
	{
		m_ui.setupUi(&m_self);

		if (m_controller->GetItemType() == ItemType::Books)
			m_ui.treeView->setHeader(new HeaderView(&m_self));

		auto & treeViewHeader = *m_ui.treeView->header();
		m_ui.treeView->setHeaderHidden(m_controller->GetItemType() == ItemType::Navigation);
		treeViewHeader.setDefaultAlignment(Qt::AlignCenter);
		m_ui.treeView->viewport()->installEventFilter(m_itemViewToolTipper.get());

		SetupNewItemButton();

		if (m_controller->GetItemType() == ItemType::Navigation)
		{
			m_delegate.reset(m_uiFactory->CreateTreeViewDelegateNavigation(*m_ui.treeView));
		}
		else
		{
			m_delegate.reset(m_uiFactory->CreateTreeViewDelegateBooks(*m_ui.treeView));
			treeViewHeader.setSectionsClickable(true);
			treeViewHeader.setStretchLastSection(false);

			treeViewHeader.setStretchLastSection(false);
			treeViewHeader.setContextMenuPolicy(Qt::CustomContextMenu);
			connect(&treeViewHeader, &QWidget::customContextMenuRequested, &m_self, [&] (const QPoint & pos)
			{
				CreateHeaderContextMenu(pos);
			});
		}

		m_ui.treeView->setItemDelegate(m_delegate->GetDelegate());
		m_delegate->RegisterObserver(this);

		m_filterTimer.setSingleShot(true);
		m_filterTimer.setInterval(std::chrono::milliseconds(200));

		FillComboBoxes();
		Connect();

		Impl::OnModeChanged(m_controller->GetModeIndex());
		m_controller->RegisterObserver(this);

		QTimer::singleShot(0, [&]
		{
			emit m_self.NavigationModeNameChanged(m_ui.cbMode->currentData().toString());
		});
	}

	void SetupNewItemButton() const
	{
		m_ui.btnNew->setVisible(false);
		m_ui.btnNew->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		auto * icon = new QSvgWidget(m_ui.btnNew);

		QFile file(":/icons/plus.svg");
		[[maybe_unused]] const auto ok = file.open(QIODevice::ReadOnly);
		assert(ok);

		const auto content = QString::fromUtf8(file.readAll())
			.arg(0)
			.arg(Util::ToString(m_self.palette(), QPalette::Base))
			.arg(Util::ToString(m_self.palette(), QPalette::Text))
			.toUtf8();

		icon->load(content);
		m_ui.btnNew->setLayout(new QHBoxLayout(m_ui.btnNew));
		m_ui.btnNew->layout()->addWidget(icon);
	}

	void FillComboBoxes()
	{
		for (const auto * name : m_controller->GetModeNames())
			m_ui.cbMode->addItem(Loc::Tr(m_controller->TrContext(), name), QString(name));

		const auto it = std::ranges::find_if(ModeComboBox::VALUE_MODES, [mode = m_settings->Get(GetValueModeKey()).toString()] (const auto & item)
		{
			return mode == item.first;
		});
		if (it != std::cend(ModeComboBox::VALUE_MODES))
			m_ui.cbValueMode->setCurrentIndex(static_cast<int>(std::distance(std::cbegin(ModeComboBox::VALUE_MODES), it)));

		m_recentMode = m_ui.cbMode->currentData().toString();
	}

	void Connect()
	{
		connect(m_ui.cbMode, &QComboBox::currentIndexChanged, &m_self, [&] (const int)
		{
			auto newMode = m_ui.cbMode->currentData().toString();
			emit m_self.NavigationModeNameChanged(newMode);
			m_controller->SetMode(newMode);
			SaveHeaderLayout();
			m_recentMode = std::move(newMode);
		});
		connect(m_ui.cbValueMode, &QComboBox::currentIndexChanged, &m_self, [&]
		{
			m_settings->Set(GetValueModeKey(), m_ui.cbValueMode->currentData());
			m_ui.treeView->model()->setData({}, {}, Role::TextFilter);
			OnValueChanged();
		});
		connect(m_ui.value, &QLineEdit::textChanged, &m_self, [&]
		{
			OnValueChanged();
		});
		connect(&m_filterTimer, &QTimer::timeout, &m_self, [&]
		{
			auto & model = *m_ui.treeView->model();
			model.setData({}, m_ui.value->text(), Role::TextFilter);
			OnCountChanged();
			if (!m_currentId.isEmpty())
				return Find(m_currentId, Role::Id);
			if (model.rowCount() != 0 && !m_ui.treeView->currentIndex().isValid())
				m_ui.treeView->setCurrentIndex(model.index(0, 0));
		});
		connect(m_ui.treeView, &QWidget::customContextMenuRequested, &m_self, [&]
		{
			m_controller->RequestContextMenu(m_ui.treeView->currentIndex(), GetContextMenuOptions(), [&] (const QString & id, const IDataItem::Ptr & item)
			{
				OnContextMenuReady(id, item);
			});
		});
		connect(m_ui.treeView, &QTreeView::doubleClicked, &m_self, [&]
		{
			m_controller->OnDoubleClicked(m_ui.treeView->currentIndex());
		});
		connect(m_ui.treeView->header(), &QHeaderView::sortIndicatorChanged, &m_self, [&] (const int logicalIndex, const Qt::SortOrder sortOrder)
		{
			m_ui.treeView->model()->setData({}, QVariant::fromValue(qMakePair(logicalIndex, sortOrder)), Role::SortOrder);
		});
	}

	void SaveHeaderLayout()
	{
		if (m_recentMode.isEmpty())
			return;

		if (const auto currentIndex = m_ui.treeView->currentIndex(); currentIndex.isValid() && !m_currentId.isEmpty())
			m_settings->Set(GetRecentIdKey(), m_currentId);

		if (m_controller->GetItemType() != ItemType::Books || m_navigationModeName.isEmpty())
			return;

		const auto * header = m_ui.treeView->header();

		SettingsGroup guard(*m_settings, GetColumnSettingsKey());
		for (int i = 1, sz = header->count(); i < sz; ++i)
		{
			if (!header->isSectionHidden(i))
				m_settings->Set(QString(COLUMN_WIDTH_LOCAL_KEY).arg(i), header->sectionSize(i));
			m_settings->Set(QString(COLUMN_INDEX_LOCAL_KEY).arg(i), header->visualIndex(i));
			m_settings->Set(QString(COLUMN_HIDDEN_LOCAL_KEY).arg(i), header->isSectionHidden(i));
		}

		m_settings->Set(SORT_INDICATOR_COLUMN_KEY, header->sortIndicatorSection());
		m_settings->Set(SORT_INDICATOR_ORDER_KEY, header->sortIndicatorOrder());
	}

	void RestoreHeaderLayout()
	{
		const ScopedCall setVisibleSectionsGuard([this] { OnHeaderSectionsVisibleChanged(); });

		if (m_recentMode.isEmpty())
			return;

		m_currentId = m_settings->Get(GetRecentIdKey(), m_currentId);

		if (m_controller->GetItemType() != ItemType::Books || m_navigationModeName.isEmpty())
			return;

		auto * header = m_ui.treeView->header();

		auto lastRestoredLayoutKey = m_ui.treeView->model()->rowCount() == 0 ? QString {} : QString("%1_%2").arg(m_navigationModeName, m_ui.cbMode->currentData().toString());
		if (m_lastRestoredLayoutKey == lastRestoredLayoutKey)
		{
			m_ui.treeView->model()->setData({}, QVariant::fromValue(qMakePair(header->sortIndicatorSection(), header->sortIndicatorOrder())), Role::SortOrder);
			return;
		}

		m_lastRestoredLayoutKey = std::move(lastRestoredLayoutKey);

		std::map<int, int> widths;
		std::map<int, int> indices;
		auto sortIndex = 0;
		auto sortOrder = Qt::AscendingOrder;
		{
			SettingsGroup guard(*m_settings, GetColumnSettingsKey());
			const auto columns = m_settings->GetGroups();
			for (const auto & column : columns)
			{
				bool ok = false;
				if (const auto logicalIndex = column.toInt(&ok); ok)
				{
					widths.try_emplace(logicalIndex, m_settings->Get(QString(COLUMN_WIDTH_LOCAL_KEY).arg(column), -1));
					indices.try_emplace(m_settings->Get(QString(COLUMN_INDEX_LOCAL_KEY).arg(column), -1), logicalIndex);
					m_settings->Get(QString(COLUMN_HIDDEN_LOCAL_KEY).arg(column), false)
						? header->hideSection(logicalIndex)
						: header->showSection(logicalIndex);
				}
			}

			if (columns.isEmpty())
				for (auto i = 0, sz = header->count(); i < sz; ++i)
					header->showSection(i);

			sortIndex = m_settings->Get(SORT_INDICATOR_COLUMN_KEY, sortIndex);
			sortOrder = m_settings->Get(SORT_INDICATOR_ORDER_KEY, sortOrder);
			m_ui.treeView->model()->setData({}, QVariant::fromValue(qMakePair(sortIndex, sortOrder)), Role::SortOrder);
		}

		indices.erase(-1);

		auto totalWidth = m_ui.treeView->viewport()->width() - QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent);

		for (int i = header->count() - 1; i > 0; --i)
		{
			const auto width = [&]
			{
				if (const auto it = widths.find(i); it != widths.end())
				{
					header->resizeSection(i, it->second);
					return it->second;
				}

				return header->sectionSize(i);
			}();

			if (!header->isSectionHidden(i))
				totalWidth -= width;
		}

		header->resizeSection(0, totalWidth);

		for (const auto [visualIndex, logicalIndex] : indices)
			header->moveSection(header->visualIndex(logicalIndex), visualIndex);

		header->setSortIndicator(sortIndex, sortOrder);
	}

	void OnHeaderSectionsVisibleChanged() const
	{
		QVector<int> visibleSections;
		const auto * header = m_ui.treeView->header();
		for (int i = 0, sz = header->count(); i < sz; ++i)
			if (!header->isSectionHidden(i))
				visibleSections << i;
		m_ui.treeView->model()->setData({}, QVariant::fromValue(visibleSections), Role::VisibleColumns);
	}

	void CreateHeaderContextMenu(const QPoint & pos)
	{
		const auto column = BookItem::Remap(m_ui.treeView->header()->logicalIndexAt(pos));
		(column == BookItem::Column::Lang ? GetLanguageContextMenu() : GetHeaderContextMenu())->exec(m_ui.treeView->header()->mapToGlobal(pos));
	}

	std::shared_ptr<QMenu> GetHeaderContextMenu()
	{
		auto menu = std::make_shared<QMenu>();
		auto * header = m_ui.treeView->header();
		const auto * model = header->model();
		for (int i = 1, sz = header->count(); i < sz; ++i)
		{
			const auto index = header->logicalIndex(i);
			auto * action = menu->addAction(model->headerData(index, Qt::Horizontal, Role::HeaderTitle).toString(), &m_self, [this_ = this, header, index] (const bool checked)
			{
				if (!checked)
					header->resizeSection(0, header->sectionSize(0) + header->sectionSize(index));
				header->setSectionHidden(index, !checked);
				if (checked)
					header->resizeSection(0, header->sectionSize(0) - header->sectionSize(index));

				this_->SaveHeaderLayout();
				this_->OnHeaderSectionsVisibleChanged();
			});
			action->setCheckable(true);
			action->setChecked(!header->isSectionHidden(index));
		}
		return menu;
	}

	std::shared_ptr<QMenu> GetLanguageContextMenu()
	{
		if (m_languageContextMenu)
			return m_languageContextMenu;

		const auto languageFilter = m_ui.treeView->model()->data({}, Role::LanguageFilter).toString();
		auto languages = m_ui.treeView->model()->data({}, Role::Languages).toStringList();
		if (languages.size() < 2)
			return GetHeaderContextMenu();

		m_languageContextMenu = std::make_unique<QMenu>();
		auto * menuGroup = new QActionGroup(m_languageContextMenu.get());

		languages.push_front("");
		if (auto recentLanguage = m_settings->Get(RECENT_LANG_FILTER_KEY).toString(); !recentLanguage.isEmpty() && languages.contains(recentLanguage))
			languages.push_front(std::move(recentLanguage));

		for (const auto & language : languages)
		{
			auto * action = m_languageContextMenu->addAction(language, &m_self, [&, language]
			{
				m_ui.treeView->model()->setData({}, language, Role::LanguageFilter);
				OnCountChanged();
				m_settings->Set(RECENT_LANG_FILTER_KEY, language);
			});
			action->setCheckable(true);
			action->setChecked(language == languageFilter);
			menuGroup->addAction(action);
		}

		return m_languageContextMenu;
	}

	void SetLanguageFilter() const
	{
		if (!m_settings->Get(Constant::Settings::KEEP_RECENT_LANG_FILTER_KEY, false))
			return;

		if (const auto language = m_settings->Get(RECENT_LANG_FILTER_KEY, QString {}); !language.isEmpty())
			m_ui.treeView->model()->setData({}, language, Role::LanguageFilter);
	}

	void OnValueChanged()
	{
		((*this).*ModeComboBox::VALUE_MODES[m_ui.cbValueMode->currentIndex()].second)();
	}

	void Find(const QVariant & value, const int role) const
	{
		const auto & model = *m_ui.treeView->model();
		if (const auto matched = model.match(model.index(0, 0), role, value, 1, (role == Role::Id ? Qt::MatchFlag::MatchExactly : Qt::MatchFlag::MatchStartsWith) | Qt::MatchFlag::MatchRecursive); !matched.isEmpty())
			m_ui.treeView->setCurrentIndex(matched.front());
		else if (role == Role::Id)
			m_ui.treeView->setCurrentIndex(model.index(0, 0));

		if (const auto index = m_ui.treeView->currentIndex(); index.isValid())
			m_ui.treeView->scrollTo(index, QAbstractItemView::ScrollHint::PositionAtCenter);
	}

	void OnCountChanged() const
	{
		m_ui.lblCount->setText(m_ui.treeView->model()->data({}, Role::Count).toString());
	}

	QString GetColumnSettingsKey(const char * value = nullptr) const
	{
		return QString("ui/%1/%2/Columns/%3%4")
			.arg(m_controller->TrContext())
			.arg(m_recentMode)
			.arg(m_navigationModeName)
			.arg(value ? QString("/%1").arg(value) : QString {})
			;
	}

	QString GetValueModeKey() const
	{
		return QString(VALUE_MODE_KEY).arg(m_controller->TrContext());
	}

	QString GetRecentIdKey() const
	{
		auto key = QString("Collections/%1/%2%3/LastId")
			.arg(m_currentCollectionId)
			.arg(m_controller->TrContext())
			.arg(m_controller->GetItemType() == ItemType::Navigation ? QString("/%1").arg(m_recentMode) : QString {})
			;

		return key;
	}

private:
	TreeView & m_self;
	PropagateConstPtr<ITreeViewController, std::shared_ptr> m_controller;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<IUiFactory, std::shared_ptr> m_uiFactory;
	PropagateConstPtr<ItemViewToolTipper, std::shared_ptr> m_itemViewToolTipper;
	PropagateConstPtr<ITreeViewDelegate, std::shared_ptr> m_delegate;
	const QString m_currentCollectionId;
	Ui::TreeView m_ui {};
	QTimer m_filterTimer;
	QString m_navigationModeName;
	QString m_recentMode;
	QString m_currentId;
	std::shared_ptr<QMenu> m_languageContextMenu;
	bool m_showRemoved { false };
	QString m_lastRestoredLayoutKey;
	ITreeViewController::RemoveItems m_removeItems;
};

TreeView::TreeView(std::shared_ptr<ISettings> settings
	, std::shared_ptr<IUiFactory> uiFactory
	, std::shared_ptr<ItemViewToolTipper> itemViewToolTipper
	, const std::shared_ptr<ICollectionController> & collectionController
	, QWidget * parent
)
	: QWidget(parent)
	, m_impl(*this
		, std::move(settings)
		, std::move(uiFactory)
		, std::move(itemViewToolTipper)
		, collectionController
	)
{
	PLOGD << "TreeView created";
}

TreeView::~TreeView()
{
	PLOGD << "TreeView destroyed";
}

void TreeView::SetNavigationModeName(QString navigationModeName)
{
	m_impl->SetNavigationModeName(std::move(navigationModeName));
}

void TreeView::ShowRemoved(const bool showRemoved)
{
	m_impl->ShowRemoved(showRemoved);
}

QAbstractItemView * TreeView::GetView() const
{
	return m_impl->GetView();
}

void TreeView::FillMenu(QMenu & menu)
{
	m_impl->FillContextMenu(menu);
}

void TreeView::resizeEvent(QResizeEvent * event)
{
	m_impl->ResizeEvent(event);
	QWidget::resizeEvent(event);
}
