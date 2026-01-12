#include "ui_TreeView.h"

#include "TreeView.h"

#include <ranges>
#include <stack>

#include <QActionGroup>
#include <QMenu>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>

#include "fnd/FindPair.h"
#include "fnd/IsOneOf.h"
#include "fnd/ScopedCall.h"
#include "fnd/algorithm.h"
#include "fnd/linear.h"

#include "interface/Localization.h"
#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"
#include "interface/constants/ObjectConnectionID.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IFilterProvider.h"
#include "interface/logic/IModelSorter.h"
#include "interface/logic/ITreeViewController.h"
#include "interface/ui/ITreeViewDelegate.h"

#include "gutil/util.h"
#include "inpx/InpxConstant.h"
#include "util/ColorUtil.h"
#include "util/ObjectsConnector.h"
#include "util/UiTimer.h"
#include "util/files.h"
#include "util/language.h"
#include "widgets/ModeComboBox.h"

#include "log.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto VALUE_MODE_KEY                     = "ui/%1/ValueMode";
constexpr auto COLUMN_WIDTH_LOCAL_KEY             = "%1/Width";
constexpr auto COLUMN_INDEX_LOCAL_KEY             = "%1/Index";
constexpr auto COLUMN_HIDDEN_LOCAL_KEY            = "%1/Hidden";
constexpr auto SORT_KEY                           = "Sort";
constexpr auto SORT_INDEX_KEY                     = "Index";
constexpr auto SORT_ORDER_KEY                     = "Order";
constexpr auto RECENT_LANG_FILTER_KEY             = "ui/language";
constexpr auto COMMON_BOOKS_TABLE_COLUMN_SETTINGS = "Preferences/CommonBooksTableColumnSettings";
constexpr auto LAST                               = "Last";

class HeaderView final : public QHeaderView
{
public:
	class IObserver // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IObserver() = default;

		virtual void               OnSortChanged()    = 0;
		virtual QAbstractItemView& GetView() noexcept = 0;
	};

public:
	HeaderView(IObserver& observer, const QString& currentId, QWidget* parent = nullptr)
		: QHeaderView(Qt::Horizontal, parent)
		, m_observer { observer }
		, m_currentId { currentId }
	{
		setFirstSectionMovable(false);
		setSectionsMovable(true);

		connect(this, &QHeaderView::sectionClicked, this, &HeaderView::OnSectionClicked);
	}

	void Save(ISettings& settings) const
	{
		settings.Remove(SORT_KEY);
		for (const auto& [name, index] : m_columns)
		{
			assert(index < m_sort.size());
			const auto key = QString("%1/%2/%3").arg(SORT_KEY, name, "%1");
			settings.Set(key.arg(SORT_INDEX_KEY), index);
			settings.Set(key.arg(SORT_ORDER_KEY), static_cast<int>(m_sort[index].second));
		}
	}

	void Load(const ISettings& settings)
	{
		m_columns.clear();
		m_sort.clear();

		std::multimap<int, std::pair<QString, Qt::SortOrder>> buffer;
		SettingsGroup                                         guard(settings, SORT_KEY);
		std::ranges::transform(settings.GetGroups(), std::inserter(buffer, buffer.end()), [&](const QString& columnName) {
			const auto key = QString("%1/%2").arg(columnName, "%1");
			return std::make_pair(
				settings.Get(key.arg(SORT_INDEX_KEY), std::numeric_limits<int>::max()),
				std::make_pair(columnName, static_cast<Qt::SortOrder>(settings.Get(key.arg(SORT_ORDER_KEY), Qt::SortOrder::AscendingOrder)))
			);
		});

		const auto nameToIndex = GetNameToIndexMapping();
		std::ranges::for_each(buffer | std::views::values, [&](auto&& item) {
			if (const auto it = nameToIndex.find(item.first); it != nameToIndex.end())
			{
				m_columns.try_emplace(std::move(item.first), m_sort.size());
				m_sort.emplace_back(it->second, item.second);
			}
		});
		ApplySort();
	}

	std::unordered_map<QString, int> GetNameToIndexMapping() const
	{
		assert(model());
		std::unordered_map<QString, int> nameToIndex;
		for (int i = 0, sz = count(); i < sz; ++i)
			nameToIndex.try_emplace(model()->headerData(i, Qt::Horizontal, Role::HeaderName).toString(), i);

		return nameToIndex;
	}

	void ApplySort() const
	{
		model()->setData({}, QVariant::fromValue(m_sort), Role::SortOrder);

		auto& view = m_observer.GetView();

		if (m_currentId.isEmpty())
			return QTimer::singleShot(0, [&] {
				view.setCurrentIndex(model()->index(0, 0));
				return view.scrollToTop();
			});

		if (const auto matched = model()->match(model()->index(0, 0), Role::Id, m_currentId, 1, Qt::MatchFlag::MatchExactly | Qt::MatchFlag::MatchRecursive); !matched.isEmpty())
			view.scrollTo(matched.front(), PositionAtCenter);
	}

private: // QHeaderView
	void paintSection(QPainter* painter, const QRect& rect, const int logicalIndex) const override
	{
		{
			const ScopedCall painterGuard(
				[=] {
					painter->save();
				},
				[=] {
					painter->restore();
				}
			);
			QHeaderView::paintSection(painter, rect, logicalIndex);
		}
		if (!model())
			return;

		PaintIcon(painter, rect, logicalIndex);

		const auto columnName = model()->headerData(logicalIndex, Qt::Horizontal, Role::HeaderName).toString();
		const auto it         = m_columns.find(columnName);
		if (it == m_columns.end())
			return;

		const ScopedCall painterGuard(
			[=] {
				painter->save();
			},
			[=] {
				painter->restore();
			}
		);
		const auto palette = QApplication::palette();
		const auto fgColor = palette.color(QPalette::Text);
		const auto bgColor = palette.color(QPalette::Button);

		QColor color = fgColor;
		color.setRedF(Linear(size_t { 0 }, fgColor.redF(), m_sort.size(), bgColor.redF())(it->second));
		color.setGreenF(Linear(size_t { 0 }, fgColor.greenF(), m_sort.size(), bgColor.greenF())(it->second));
		color.setBlueF(Linear(size_t { 0 }, fgColor.blueF(), m_sort.size(), bgColor.blueF())(it->second));

		painter->setPen(color);
		painter->setBrush(color);

		const auto size     = rect.height() / 4.0;
		const auto height   = std::sqrt(2.0) * size / 2;
		auto       triangle = QPolygonF({
            QPointF {      0.0, height },
            QPointF {     size, height },
            QPointF { size / 2,      0 },
            QPointF {      0.0, height }
        });

		assert(it->second < m_sort.size());
		if (m_sort[it->second].second == Qt::DescendingOrder)
			triangle = QTransform(1, 0, 0, -1, 0, height).map(triangle);

		painter->drawPolygon(triangle.translated(rect.right() - 4 * size / 3, size / 2));
	}

private:
	bool PaintIcon(QPainter* painter, const QRect& rect, const int logicalIndex) const
	{
		const auto iconFileName = model()->headerData(logicalIndex, orientation(), Qt::DecorationRole);
		if (!iconFileName.isValid())
			return false;

		QIcon icon(iconFileName.toString());
		icon.pixmap(QSize { rect.height(), rect.height() });
		auto       size   = 6 * QSize { rect.height(), rect.height() } / 10;
		const auto center = (rect.size() - size) / 2;
		painter->drawPixmap(rect.topLeft() + QPoint { center.width(), center.height() }, icon.pixmap(size));

		return true;
	}

	void OnSectionClicked(const int logicalIndex)
	{
		if (!model())
			return;

		const ScopedCall apply([this] {
			ApplySort();
			m_observer.OnSortChanged();
		});

		auto name = model()->headerData(logicalIndex, Qt::Horizontal, Role::HeaderName).toString();
		assert(!name.isEmpty());
		const auto add = [&] {
			m_columns.try_emplace(std::move(name), std::size(m_sort));
			m_sort.emplace_back(logicalIndex, Qt::SortOrder::AscendingOrder);
		};

		const auto it = m_columns.find(name);

		const auto change = [&] {
			assert(it->second < m_sort.size());
			auto& sort = m_sort[it->second];

			if (sort.second == Qt::SortOrder::AscendingOrder)
				return (void)(sort.second = Qt::SortOrder::DescendingOrder);

			m_sort.erase(std::next(m_sort.begin(), static_cast<ptrdiff_t>(it->second)));
			std::ranges::for_each(
				m_columns | std::views::values | std::views::filter([n = it->second](const auto item) {
					return item > n;
				}),
				[](auto& item) {
					--item;
				}
			);
			m_columns.erase(it);
		};

		if (QGuiApplication::queryKeyboardModifiers() & (Qt::KeyboardModifier::ShiftModifier | Qt::KeyboardModifier::AltModifier | Qt::KeyboardModifier::ControlModifier))
			return it == m_columns.end() ? add() : change();

		if (it != m_columns.end() && it->second + 1 == m_sort.size())
			return change();

		m_sort.clear();
		m_columns.clear();
		return add();
	}

private:
	IObserver&                                 m_observer;
	const QString&                             m_currentId;
	std::vector<std::pair<int, Qt::SortOrder>> m_sort;
	std::unordered_map<QString, size_t>        m_columns;
};

class ValueEventFilter final : public QObject
{
public:
	ValueEventFilter(const TreeView& view, const QWidget& widget, QObject* parent = nullptr)
		: QObject(parent)
		, m_view { view }
		, m_widget { widget }
	{
	}

private: // QObject
	bool eventFilter(QObject* /*watched*/, QEvent* event) override
	{
		if (event->type() == QEvent::Resize)
			emit m_view.ValueGeometryChanged(Util::GetGlobalGeometry(m_widget));

		return false;
	}

private:
	const TreeView& m_view;
	const QWidget&  m_widget;
};

class MenuEventFilter final : public QObject
{
public:
	explicit MenuEventFilter(QObject* parent = nullptr)
		: QObject(parent)
	{
		m_timer.setSingleShot(true);
		m_timer.setInterval(std::chrono::seconds(2));
		connect(&m_timer, &QTimer::timeout, [this] {
			m_text.clear();
		});
	}

private: // QObject
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		if (event->type() != QEvent::KeyPress)
			return QObject::eventFilter(watched, event);

		const auto text = static_cast<const QKeyEvent*>(event)->text(); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
		if (text.isEmpty())
			return false;

		auto* menu = qobject_cast<QMenu*>(watched);
		if (!menu)
			return false;

		m_text.append(text);
		m_timer.start();

		const auto actions = menu->actions();
		if (const auto it = std::ranges::find_if(
				actions,
				[this](const QAction* action) {
					return action->text().startsWith(m_text, Qt::CaseInsensitive);
				}
			);
		    it != actions.end())
			menu->setActiveAction(*it);

		return false;
	}

private:
	QTimer  m_timer;
	QString m_text;
};

class ArchiveSorter final : public IModelSorter
{
	auto sortable(const QModelIndex& index) const
	{
		auto       value = index.data().toString();
		const auto match = m_rx.match(value);
		const auto n     = match.hasMatch() ? match.captured(1).toInt() : std::numeric_limits<int>::max();
		return std::make_pair(n, std::move(value));
	}

private: // IModelSorter
	bool LessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight, int) const override
	{
		return sortable(sourceLeft) > sortable(sourceRight);
	}

private:
	QRegularExpression m_rx { "^.*?fb2.*?([0-9]+).*?$" };
};

void TreeOperation(const QAbstractItemModel& model, const QModelIndex& index, const std::function<void(const QModelIndex&)>& f)
{
	f(index);
	for (int row = 0, sz = model.rowCount(index); row < sz; ++row)
		TreeOperation(model, model.index(row, 0, index), f);
}

} // namespace

class TreeView::Impl final
	: ITreeViewController::IObserver
	, ITreeViewDelegate::IObserver
	, IFilterProvider::IObserver
	, ModeComboBox::IValueApplier
	, HeaderView::IObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(
		TreeView&                                  self,
		const IDatabaseUser&                       databaseUser,
		std::shared_ptr<const ICollectionProvider> collectionProvider,
		std::shared_ptr<ISettings>                 settings,
		std::shared_ptr<IUiFactory>                uiFactory,
		std::shared_ptr<IFilterProvider>           filterProvider,
		std::shared_ptr<ItemViewToolTipper>        itemViewToolTipper,
		std::shared_ptr<ScrollBarController>       scrollBarController
	)
		: m_self { self }
		, m_controller { uiFactory->GetTreeViewController() }
		, m_collectionProvider { std::move(collectionProvider) }
		, m_settings { std::move(settings) }
		, m_uiFactory { std::move(uiFactory) }
		, m_filterProvider { std::move(filterProvider) }
		, m_itemViewToolTipper { std::move(itemViewToolTipper) }
		, m_scrollBarController { std::move(scrollBarController) }
		, m_delegate { std::shared_ptr<ITreeViewDelegate>() }
		, m_hiddenColumns { databaseUser.GetSetting(IDatabaseUser::Key::DisabledBookFields).toString().split(LIST_SEPARATOR, Qt::SkipEmptyParts) }
	{
		Setup();
		m_scrollBarController->SetScrollArea(m_ui.treeView);
	}

	~Impl() override
	{
		m_filterProvider->UnregisterObserver(this);
		m_controller->UnregisterObserver(this);
		m_delegate->UnregisterObserver(this);
	}

	void SetNavigationModeName(QString navigationModeName)
	{
		m_navigationModeName = std::move(navigationModeName);
	}

	void ShowRemoved(const bool showRemoved)
	{
		m_showRemoved = showRemoved;
		auto* model   = m_ui.treeView->model();
		if (!model)
			return;

		model->setData({}, m_showRemoved, Role::ShowRemovedFilter);
		OnCountChanged();
		Find(m_currentId, Role::Id);
	}

	QAbstractItemView* GetView() const
	{
		return m_ui.treeView;
	}

	void SetMode(const int mode, const QString& id)
	{
		assert(IsNavigation());
		const auto modeIndex = m_ui.cbMode->findData(mode, Qt::UserRole + 1);
		const auto modeName  = m_ui.cbMode->itemData(modeIndex).toString();
		m_settings->Set(QString(Constant::Settings::RECENT_NAVIGATION_ID_KEY).arg(m_collectionProvider->GetActiveCollectionId()).arg(modeName), id);

		if (m_controller->GetModeIndex() != mode)
			return m_ui.cbMode->setCurrentIndex(modeIndex);

		const auto& model = *m_ui.treeView->model();
		if (const auto matched = model.match(model.index(0, 0), Role::Id, id, 1, Qt::MatchFlag::MatchExactly | Qt::MatchFlag::MatchRecursive); !matched.isEmpty())
			m_ui.treeView->setCurrentIndex(matched.front());
	}

	void OnBookTitleToSearchVisibleChanged() const
	{
		emit m_self.ValueGeometryChanged(Util::GetGlobalGeometry(*m_ui.value));
	}

	void OnCurrentNavigationItemChanged(const QModelIndex& index)
	{
		assert(!IsNavigation());
		m_navigationItemFlags = index.data(Role::Flags).value<IDataItem::Flags>();
	}

	void ResizeEvent(const QResizeEvent* event)
	{
		auto size = m_ui.cbMode->height();
		m_ui.btnNew->setMinimumSize(size, size);
		m_ui.btnNew->setMaximumSize(size, size);
		m_ui.cbValueMode->setMinimumSize(size, size);
		m_ui.cbValueMode->setMaximumSize(size, size);

		if (m_controller->GetItemType() != ItemType::Books)
			return;

		emit m_self.ValueGeometryChanged(Util::GetGlobalGeometry(*m_ui.value));

		if (m_recentMode.isEmpty() || m_navigationModeName.isEmpty())
			return;

		CheckHeaderViewWidth(*event);
	}

private: // ITreeViewController::IObserver
	void OnModeChanged(const int index) override
	{
		m_ui.value->setText({});
		const auto idIndex = m_ui.cbMode->findData(index, Qt::UserRole + 1);
		m_ui.cbMode->setCurrentIndex(std::max(idIndex, 0));
	}

	void OnModelChanged(QAbstractItemModel* model) override
	{
		if (m_ui.treeView->model() != model)
			OnModelChangedImpl(model);

		RestoreHeaderLayout();
		OnFilterEnabledChanged();

		m_ui.value->setText({});
		Find();
		OnCountChanged();
	}

	void OnContextMenuTriggered(const QString& /*id*/, const IDataItem::Ptr& item) override
	{
		switch (item->GetData(MenuItem::Column::Id).toInt())
		{
			case BooksMenuAction::Collapse:
				for (const auto& row : m_ui.treeView->selectionModel()->selectedRows())
					TreeOperation(*m_ui.treeView->model(), row, [this](const auto& index) {
						m_ui.treeView->collapse(index);
					});
				return;
			case BooksMenuAction::Expand:
				for (const auto& row : m_ui.treeView->selectionModel()->selectedRows())
					TreeOperation(*m_ui.treeView->model(), row, [this](const auto& index) {
						m_ui.treeView->expand(index);
					});
				return;
			case BooksMenuAction::CollapseAll:
				return m_ui.treeView->collapseAll();
			case BooksMenuAction::ExpandAll:
				return m_ui.treeView->expandAll();
			default:
				break;
		}

		if (IsOneOf(item->GetData(MenuItem::Column::Id).toInt(), BooksMenuAction::SendAsArchive, BooksMenuAction::SendAsIs, BooksMenuAction::SendAsScript) && item->GetData(MenuItem::Column::HasError).toInt())
			m_uiFactory->ShowWarning(Loc::Tr(Loc::Ctx::ERROR_CTX, Loc::BOOKS_EXTRACT_ERROR));
	}

private: // ITreeViewDelegate::IObserver
	void OnButtonClicked(const QModelIndex&) override
	{
		assert(m_removeItems);
		m_removeItems(m_ui.treeView->selectionModel()->selectedIndexes());
	}

private: // IFilterProvider::IObserver
	void OnFilterEnabledChanged() override
	{
		auto& model = *m_ui.treeView->model();
		model.setData({}, m_filterProvider->IsFilterEnabled(), Role::UniFilterEnabled);
		if (!IsNavigation())
		{
			model.setData({}, m_filterProvider->HideUnrated(), Role::UniFilterHideUnrated);
			model.setData({}, m_filterProvider->IsMinimumRateEnabled() ? m_filterProvider->GetMinimumRate() : QVariant {}, Role::UniFilterMinimumRate);
			model.setData({}, m_filterProvider->IsMaximumRateEnabled() ? m_filterProvider->GetMaximumRate() : QVariant {}, Role::UniFilterMaximumRate);
			return;
		}

		OnCountChanged();
		m_controller->SetCurrentId(ItemType::Navigation, m_currentId, true);
	}

	void OnFilterNavigationChanged(const NavigationMode navigationMode) override
	{
		if (!IsNavigation())
			return;

		m_controller->SetCurrentId(ItemType::Navigation, m_currentId, true);

		if (m_controller->GetModeIndex() != static_cast<int>(navigationMode))
			return;

		m_ui.treeView->model()->setData({}, {}, Role::UniFilterChanged);
		OnCountChanged();
	}

	void OnFilterBooksChanged() override
	{
		if (IsNavigation())
			m_controller->SetCurrentId(ItemType::Navigation, m_currentId, true);
	}

private: //	ModeComboBox::IValueApplier
	void Find() override
	{
		if (!m_ui.value->text().isEmpty())
			return Find(m_ui.value->text(), IsNavigation() ? Qt::DisplayRole : static_cast<int>(Role::Title));

		if (!m_currentId.isEmpty())
			return Find(m_currentId, Role::Id);

		m_ui.treeView->setCurrentIndex(m_ui.treeView->model()->index(0, 0));
	}

	void Filter() override
	{
		m_filterTimer.start();
	}

private: // HeaderView::IObserver
	void OnSortChanged() override
	{
		SaveHeaderLayout();
	}

	QAbstractItemView& GetView() noexcept override
	{
		return *m_ui.treeView;
	}

private:
	bool IsNavigation() const
	{
		return m_controller->GetItemType() == ItemType::Navigation;
	}

	void OnModelChangedImpl(QAbstractItemModel* model)
	{
		m_ui.treeView->setModel(model);
		m_ui.treeView->setRootIsDecorated(m_controller->GetViewMode() == ViewMode::Tree);

		connect(m_ui.treeView->selectionModel(), &QItemSelectionModel::currentRowChanged, &m_self, [this](const QModelIndex& index, const QModelIndex& prev) {
			auto currentId = index.data(Role::Id).toString();
			m_controller->SetCurrentId(index.data(Role::Type).value<ItemType>(), currentId);
			if (prev.isValid())
				m_settings->Set(GetRecentIdKey(), m_currentId = std::move(currentId));

			if (IsNavigation())
			{
				emit m_self.CurrentNavigationItemChanged(index);
				if (m_controller->GetModeIndex() == static_cast<int>(NavigationMode::Search))
					emit m_self.SearchNavigationItemSelected(m_currentId.toLongLong(), index.data().toString());
			}
		});

		if (IsNavigation())
		{
			m_delegate->SetEnabled(static_cast<bool>((m_removeItems = m_controller->GetRemoveItems())));
			if (m_controller->GetModeIndex() == static_cast<int>(NavigationMode::Archives))
			{
				model->setData({}, QVariant::fromValue<const IModelSorter*>(&m_archiveSorter), Role::ModelSorter);
				std::vector<std::pair<int, Qt::SortOrder>> sort {
					{ 0, Qt::SortOrder::AscendingOrder }
				};
				model->setData({}, QVariant::fromValue(std::move(sort)), Role::SortOrder);
			}
		}
		else
		{
			m_languageContextMenu.reset();
			model->setData({}, true, Role::Checkable);
			model->setData({}, !!(m_navigationItemFlags & (IDataItem::Flags::Filtered | IDataItem::Flags::BooksFiltered)), Role::NavigationItemFiltered);
		}
		model->setData({}, m_showRemoved, Role::ShowRemovedFilter);

		m_delegate->OnModelChanged();

		const auto modelEmpty = model->rowCount() == 0;

		if (auto newItemCreator = m_controller->GetNewItemCreator(); !newItemCreator)
		{
			m_ui.btnNew->setVisible(false);
		}
		else
		{
			m_ui.btnNew->setVisible(true);
			m_ui.btnNew->disconnect(SIGNAL(clicked()));
			connect(m_ui.btnNew, &QAbstractButton::clicked, &m_self, std::move(newItemCreator));

			if (modelEmpty)
				QTimer::singleShot(1000, [this] {
					ShowPushMe();
				});
		}

		m_ui.value->setEnabled(!modelEmpty);
		if (modelEmpty)
			m_controller->SetCurrentId(ItemType::Unknown, {});
	}

	void ShowPushMe()
	{
		if (!m_ui.btnNew->isVisible())
			return;

		auto* timer = new QTimer(&m_self);
		timer->setSingleShot(false);
		timer->setInterval(std::chrono::milliseconds(200));
		connect(timer, &QObject::destroyed, m_ui.btnNew, [this] {
			m_ui.btnNew->setAutoRaise(true);
			m_ui.value->setText({});
		});
		connect(timer, &QTimer::timeout, m_ui.btnNew, [this, timer, n = 0]() mutable {
			m_ui.value->setText(n % 2 ? QString() : QString("%1 %2").arg(QChar(0x2B60)).arg(tr("Push me")));
			m_ui.value->setCursorPosition(0);

			m_ui.btnNew->setAutoRaise(n % 2);
			if (++n == 15)
				timer->deleteLater();
		});
		timer->start();
	}

	ITreeViewController::RequestContextMenuOptions GetContextMenuOptions() const
	{
		static constexpr auto hasCollapsedExpanded = ITreeViewController::RequestContextMenuOptions::HasExpanded | ITreeViewController::RequestContextMenuOptions::HasCollapsed;

		const auto& model = *m_ui.treeView->model();

		const auto addOption = [](const bool condition, const ITreeViewController::RequestContextMenuOptions option) {
			return condition ? option : ITreeViewController::RequestContextMenuOptions::None;
		};

		const auto currentIndex = m_ui.treeView->currentIndex();

		ITreeViewController::RequestContextMenuOptions options =
			addOption(model.data({}, Role::IsTree).toBool(), ITreeViewController::RequestContextMenuOptions::IsTree)
			| addOption(m_ui.treeView->selectionModel()->hasSelection(), ITreeViewController::RequestContextMenuOptions::HasSelection)
			| addOption(m_showRemoved, ITreeViewController::RequestContextMenuOptions::ShowRemoved)
			| addOption(m_collectionProvider->GetActiveCollection().destructiveOperationsAllowed, ITreeViewController::RequestContextMenuOptions::AllowDestructiveOperations)
			| addOption(m_filterProvider->IsFilterEnabled(), ITreeViewController::RequestContextMenuOptions::UniFilterEnabled)
			| addOption(
				currentIndex.isValid() && currentIndex.data(Role::Type).value<ItemType>() == ItemType::Books && Zip::IsArchive(Util::RemoveIllegalPathCharacters(currentIndex.data(Role::FileName).toString())),
				ITreeViewController::RequestContextMenuOptions::IsArchive
			);

		if (!!(options & ITreeViewController::RequestContextMenuOptions::IsTree))
		{
			const auto checkIndex = [&](const QModelIndex& index, const ITreeViewController::RequestContextMenuOptions onExpanded, const ITreeViewController::RequestContextMenuOptions onCollapsed) {
				assert(index.isValid());
				const auto result = model.rowCount(index) > 0;
				if (result)
					options |= m_ui.treeView->isExpanded(index) ? onExpanded : onCollapsed;
				return result;
			};

			if (currentIndex.isValid())
				checkIndex(currentIndex, ITreeViewController::RequestContextMenuOptions::NodeExpanded, ITreeViewController::RequestContextMenuOptions::NodeCollapsed);

			std::stack<QModelIndex> stack { { QModelIndex {} } };
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

	void OnContextMenuReady(const QString& id, const IDataItem::Ptr& item)
	{
		if (m_ui.treeView->currentIndex().data(Role::Id).toString() != id)
			return;

		QMenu menu;
		menu.setFont(m_self.font());
		GenerateMenu(menu, *item);
		if (!menu.isEmpty())
			menu.exec(QCursor::pos());
	}

	void GenerateMenu(QMenu& menu, const IDataItem& item)
	{
		const auto                                      font = menu.font();
		const QFontMetrics                              metrics(font);
		std::stack<std::pair<const IDataItem*, QMenu*>> stack { { { &item, &menu } } };

		const auto getBool = [](const IDataItem& menuItem, const int column, const bool defaultValue) {
			const auto& str = menuItem.GetData(column);
			return str.isEmpty() ? defaultValue : QVariant(str).toBool();
		};

		while (!stack.empty())
		{
			auto [parent, subMenu] = stack.top();
			stack.pop();

			auto maxWidth = subMenu->minimumWidth();

			for (size_t i = 0, sz = parent->GetChildCount(); i < sz; ++i)
			{
				auto       child     = parent->GetChild(i);
				const auto enabled   = getBool(*child, MenuItem::Column::Enabled, true);
				const auto title     = child->GetData().toStdString();
				const auto titleText = Loc::Tr(m_controller->TrContext(), title.data());
				auto       statusTip = titleText;
				statusTip.replace("&", "");
				maxWidth = std::max(maxWidth, metrics.boundingRect(statusTip).width() + 3 * (metrics.ascent() + metrics.descent()));

				if (child->GetChildCount() != 0)
				{
					auto* subSubMenu = stack.emplace(child.get(), subMenu->addMenu(titleText)).second;
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

				const auto checkable = getBool(*child, MenuItem::Column::Checkable, false);
				const auto checked   = getBool(*child, MenuItem::Column::Checked, false);

				auto* action = subMenu->addAction(titleText, [&, child = std::move(child)]() mutable {
					const auto& view = *m_ui.treeView;

					auto selected           = view.selectionModel()->selectedIndexes();
					const auto [begin, end] = std::ranges::remove_if(selected, [](const auto& index) {
						return index.column() != 0;
					});
					selected.erase(begin, end);
					m_controller->OnContextMenuTriggered(view.model(), view.currentIndex(), selected, std::move(child));
				});
				action->setStatusTip(statusTip);
				action->setEnabled(enabled);
				action->setCheckable(checkable);
				if (checkable)
					action->setChecked(checked);
			}

			subMenu->setMinimumWidth(maxWidth);
			if (parent->GetChildCount() > 16)
				subMenu->installEventFilter(&m_menuEventFilter);
		}
	}

	void Setup()
	{
		m_ui.setupUi(&m_self);

		if (!IsNavigation())
		{
			m_booksHeaderView = new HeaderView(*this, m_currentId, &m_self);
			m_ui.treeView->setHeader(m_booksHeaderView);
			connect(m_booksHeaderView, &QHeaderView::sectionResized, &m_self, [this] {
				SaveHeaderLayout();
			});
			connect(m_booksHeaderView, &QHeaderView::sectionMoved, &m_self, [this] {
				SaveHeaderLayout();
			});
		}

		auto& treeViewHeader = *m_ui.treeView->header();
		m_ui.treeView->setHeaderHidden(IsNavigation());
		treeViewHeader.setDefaultAlignment(Qt::AlignCenter);
		m_ui.treeView->viewport()->installEventFilter(m_itemViewToolTipper.get());
		m_ui.treeView->viewport()->installEventFilter(m_scrollBarController.get());

		SetupNewItemButton();

		if (IsNavigation())
		{
			m_delegate.reset(m_uiFactory->CreateTreeViewDelegateNavigation(*m_ui.treeView));

			Util::ObjectsConnector::registerEmitter(ObjectConnectorID::SEARCH_NAVIGATION_ITEM_SELECTED, &m_self, SIGNAL(SearchNavigationItemSelected(long long, const QString&)));
			Util::ObjectsConnector::registerEmitter(ObjectConnectorID::CURRENT_NAVIGATION_ITEM_CHANGED, &m_self, SIGNAL(CurrentNavigationItemChanged(const QModelIndex&)));
		}
		else
		{
			m_delegate.reset(m_uiFactory->CreateTreeViewDelegateBooks(*m_ui.treeView));
			treeViewHeader.setSectionsClickable(true);
			treeViewHeader.setStretchLastSection(false);

			treeViewHeader.setStretchLastSection(false);
			treeViewHeader.setContextMenuPolicy(Qt::CustomContextMenu);
			connect(&treeViewHeader, &QWidget::customContextMenuRequested, &m_self, [&](const QPoint& pos) {
				CreateHeaderContextMenu(pos);
			});

			m_ui.value->installEventFilter(new ValueEventFilter(m_self, *m_ui.value, m_ui.value));

			Util::ObjectsConnector::registerEmitter(ObjectConnectorID::BOOKS_SEARCH_FILTER_VALUE_GEOMETRY_CHANGED, &m_self, SIGNAL(ValueGeometryChanged(const QRect&)));
			Util::ObjectsConnector::registerReceiver(ObjectConnectorID::BOOK_TITLE_TO_SEARCH_VISIBLE_CHANGED, &m_self, SLOT(OnBookTitleToSearchVisibleChanged()));
			Util::ObjectsConnector::registerReceiver(ObjectConnectorID::CURRENT_NAVIGATION_ITEM_CHANGED, &m_self, SLOT(OnCurrentNavigationItemChanged(const QModelIndex&)));
		}

		m_ui.value->setAccessibleName(QString("%1SearchAndFilter").arg(m_controller->GetItemName()));

		m_ui.treeView->setItemDelegate(m_delegate->GetDelegate());
		m_delegate->RegisterObserver(this);

		m_filterTimer.setSingleShot(true);
		m_filterTimer.setInterval(std::chrono::milliseconds(200));

		FillComboBoxes();
		Connect();

		Impl::OnModeChanged(m_controller->GetModeIndex());
		m_controller->RegisterObserver(this);
		m_filterProvider->RegisterObserver(this);

		QTimer::singleShot(0, [&] {
			emit m_self.NavigationModeNameChanged(m_ui.cbMode->currentData().toString());
		});
	}

	void SetupNewItemButton() const
	{
		m_ui.btnNew->setVisible(false);
		m_ui.btnNew->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		m_ui.btnNew->setIcon(QIcon(":/icons/plus.svg"));
	}

	void FillComboBoxes()
	{
		for (const auto& [name, id] : m_controller->GetModeNames())
		{
			m_ui.cbMode->addItem(Loc::Tr(m_controller->TrContext(), name), QString(name));
			m_ui.cbMode->setItemData(m_ui.cbMode->count() - 1, id, Qt::UserRole + 1);
		}
		m_ui.cbMode->setCurrentIndex(-1);

		const auto it = std::ranges::find_if(ModeComboBox::VALUE_MODES, [mode = m_settings->Get(GetValueModeKey()).toString()](const auto& item) {
			return mode == item.first;
		});
		if (it != std::cend(ModeComboBox::VALUE_MODES))
			m_ui.cbValueMode->setCurrentIndex(static_cast<int>(std::distance(std::cbegin(ModeComboBox::VALUE_MODES), it)));

		m_recentMode = m_ui.cbMode->currentData().toString();
	}

	void Connect()
	{
		connect(m_ui.cbMode, &QComboBox::currentIndexChanged, &m_self, [&](const int) {
			auto newMode = m_ui.cbMode->currentData().toString();
			emit m_self.NavigationModeNameChanged(newMode);
			m_recentMode = std::move(newMode);
			m_controller->SetMode(m_recentMode);
			m_ui.value->setFocus(Qt::FocusReason::OtherFocusReason);
		});
		connect(m_ui.cbValueMode, &QComboBox::currentIndexChanged, &m_self, [&] {
			m_settings->Set(GetValueModeKey(), m_ui.cbValueMode->currentData());
			m_ui.treeView->model()->setData({}, {}, Role::TextFilter);
			OnValueChanged();
			m_ui.value->setFocus(Qt::FocusReason::OtherFocusReason);
		});
		connect(m_ui.value, &QLineEdit::textChanged, &m_self, [&] {
			OnValueChanged();
		});
		connect(&m_filterTimer, &QTimer::timeout, &m_self, [&] {
			auto& model = *m_ui.treeView->model();
			model.setData({}, m_ui.value->text(), Role::TextFilter);
			OnCountChanged();
			if (!m_currentId.isEmpty())
				return Find(m_currentId, Role::Id);
			if (model.rowCount() != 0 && !m_ui.treeView->currentIndex().isValid())
				m_ui.treeView->setCurrentIndex(model.index(0, 0));
		});
		connect(m_ui.treeView, &QWidget::customContextMenuRequested, &m_self, [&] {
			m_controller->RequestContextMenu(m_ui.treeView->currentIndex(), GetContextMenuOptions(), [&](const QString& id, const IDataItem::Ptr& item) {
				OnContextMenuReady(id, item);
			});
		});
		connect(m_ui.treeView, &QTreeView::doubleClicked, &m_self, [&] {
			m_controller->OnDoubleClicked(m_ui.treeView->currentIndex());
		});
	}

	void SaveHeaderLayout()
	{
		if (m_recentMode.isEmpty())
			return;

		if (m_controller->GetItemType() != ItemType::Books || m_navigationModeName.isEmpty())
			return;

		const auto* header           = m_ui.treeView->header();
		const auto* model            = header->model();
		const auto  saveHeaderLayout = [&] {
            for (int i = 0, sz = header->count(); i < sz; ++i)
            {
                const auto name = model->headerData(i, Qt::Horizontal, Role::HeaderName).toString();
                if (!header->isSectionHidden(i))
                    m_settings->Set(QString(COLUMN_WIDTH_LOCAL_KEY).arg(name), header->sectionSize(i));
                m_settings->Set(QString(COLUMN_INDEX_LOCAL_KEY).arg(name), header->visualIndex(i));
                m_settings->Set(QString(COLUMN_HIDDEN_LOCAL_KEY).arg(name), header->isSectionHidden(i));
            }

            m_booksHeaderView->Save(*m_settings);
		};

		if (!m_settings->Get(COMMON_BOOKS_TABLE_COLUMN_SETTINGS, false))
		{
			SettingsGroup guard(*m_settings, GetColumnSettingsKey());
			saveHeaderLayout();
		}
		{
			SettingsGroup guard(*m_settings, GetColumnSettingsKey(nullptr, LAST));
			saveHeaderLayout();
		}
	}

	void RestoreHeaderLayout()
	{
		const ScopedCall setVisibleSectionsGuard([this] {
			OnHeaderSectionsVisibleChanged();
		});

		if (m_recentMode.isEmpty())
			return;

		m_currentId = m_settings->Get(GetRecentIdKey(), m_currentId);

		UpdateSectionSize();
		if (m_controller->GetItemType() != ItemType::Books || m_navigationModeName.isEmpty())
			return;

		auto* header = m_ui.treeView->header();

		auto lastRestoredLayoutKey = m_ui.treeView->model()->rowCount() == 0 ? QString {} : QString("%1_%2").arg(m_navigationModeName, m_ui.cbMode->currentData().toString());
		if (!Util::Set(m_lastRestoredLayoutKey, lastRestoredLayoutKey))
			return m_booksHeaderView->ApplySort();

		const auto nameToIndex = m_booksHeaderView->GetNameToIndexMapping();

		std::map<int, int>          widths;
		std::multimap<int, QString> indices;

		QSignalBlocker resizeGuard(header);

		const auto collectData = [&] {
			const auto columns = m_settings->GetGroups();
			for (const auto& columnName : columns)
			{
				const auto it = nameToIndex.find(columnName);
				if (it == nameToIndex.end())
					continue;

				const auto logicalIndex = it->second;
				widths.try_emplace(logicalIndex, m_settings->Get(QString(COLUMN_WIDTH_LOCAL_KEY).arg(columnName), -1));
				indices.emplace(m_settings->Get(QString(COLUMN_INDEX_LOCAL_KEY).arg(columnName), std::numeric_limits<int>::max()), columnName);
				m_hiddenColumns.contains(columnName, Qt::CaseInsensitive) || m_settings->Get(QString(COLUMN_HIDDEN_LOCAL_KEY).arg(columnName), false) ? header->hideSection(logicalIndex)
																																					  : header->showSection(logicalIndex);
			}

			m_booksHeaderView->Load(*m_settings);
		};

		if (!m_settings->Get(COMMON_BOOKS_TABLE_COLUMN_SETTINGS, false))
		{
			SettingsGroup guard(*m_settings, GetColumnSettingsKey());
			collectData();
		}
		if (widths.empty())
		{
			SettingsGroup guard(*m_settings, GetColumnSettingsKey(nullptr, LAST));
			collectData();
		}

		if (widths.empty())
			for (auto i = 0, sz = header->count(); i < sz; ++i)
				header->showSection(i);

		for (int i = 0, sz = header->count(); i < sz; ++i)
			if (const auto it = widths.find(i); it != widths.end())
				header->resizeSection(i, it->second);

		auto absent = nameToIndex;
		for (const auto& columnName : indices | std::views::values)
			absent.erase(columnName);
		for (const auto& [name, index] : absent)
			if (index > 0 && index < std::ssize(indices))
				indices.emplace_hint(indices.begin(), std::next(indices.begin(), index - 1)->first, name);

		for (int n = 0; const auto& columnName : indices | std::views::values)
			if (const auto it = nameToIndex.find(columnName); it != nameToIndex.end())
				header->moveSection(header->visualIndex(it->second), ++n);
		header->moveSection(header->visualIndex(0), 0);

		CheckHeaderViewWidth(QResizeEvent(m_self.size(), m_self.size()));
	}

	void CheckHeaderViewWidth(const QResizeEvent& event)
	{
		const auto diff   = m_ui.treeView->width() - m_ui.treeView->viewport()->width();
		auto&      header = *m_ui.treeView->header();
		if (const auto length = header.length() + diff; std::abs(length - event.oldSize().width()) < 3 * QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent))
		{
			if (const auto offset = event.size().width() - length)
			{
				QSignalBlocker block(&header);
				header.resizeSection(0, m_ui.treeView->header()->sectionSize(0) + offset);
				SaveHeaderLayout();
			}
		}
	}

	void OnHeaderSectionsVisibleChanged() const
	{
		QVector<int> visibleSections;
		const auto*  header = m_ui.treeView->header();
		for (int i = 0, sz = header->count(); i < sz; ++i)
			if (!header->isSectionHidden(i))
				visibleSections << i;
		m_ui.treeView->model()->setData({}, QVariant::fromValue(visibleSections), Role::VisibleColumns);
	}

	void CreateHeaderContextMenu(const QPoint& pos)
	{
		const auto column      = BookItem::Remap(m_ui.treeView->header()->logicalIndexAt(pos));
		const auto contextMenu = column == BookItem::Column::Lang ? GetLanguageContextMenu() : GetHeaderContextMenu();
		contextMenu->setFont(m_self.font());
		contextMenu->exec(m_ui.treeView->header()->mapToGlobal(pos));
	}

	std::shared_ptr<QMenu> GetHeaderContextMenu()
	{
		auto        menu   = std::make_shared<QMenu>();
		auto*       header = m_ui.treeView->header();
		const auto* model  = header->model();
		for (int i = 1, sz = header->count(); i < sz; ++i)
		{
			const auto index = header->logicalIndex(i);
			if (m_hiddenColumns.contains(model->headerData(index, Qt::Horizontal, Role::HeaderName).toString(), Qt::CaseInsensitive))
				continue;

			auto* action = menu->addAction(model->headerData(index, Qt::Horizontal, Role::HeaderTitle).toString(), &m_self, [this_ = this, header, index](const bool checked) {
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
		auto       languages      = m_ui.treeView->model()->data({}, Role::Languages).toStringList();
		if (languages.size() < 2)
			return GetHeaderContextMenu();

		m_languageContextMenu = std::make_unique<QMenu>();
		auto* menuGroup       = new QActionGroup(m_languageContextMenu.get());

		languages.push_front("");
		auto sortBeginIndex = 1;

		if (auto recentLanguage = m_settings->Get(RECENT_LANG_FILTER_KEY).toString(); !recentLanguage.isEmpty() && languages.contains(recentLanguage))
		{
			languages.push_front(recentLanguage);
			sortBeginIndex = 2;
		}

		std::vector<std::pair<QString, QString>> languageTranslated;
		languageTranslated.reserve(languages.size());
		std::ranges::transform(std::move(languages), std::back_inserter(languageTranslated), [translations = GetLanguagesMap()](QString& language) {
			const auto it         = translations.find(language);
			auto       translated = it != translations.end() ? Loc::Tr(LANGUAGES_CONTEXT, it->second) : language;
			return std::make_pair(std::move(language), std::move(translated));
		});
		std::ranges::sort(languageTranslated | std::views::drop(sortBeginIndex), {}, [](const auto& item) {
			return item.second;
		});

		for (const auto& [language, translated] : languageTranslated)
		{
			auto* action = m_languageContextMenu->addAction(translated, &m_self, [&, language] {
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

	void OnValueChanged()
	{
		std::invoke(ModeComboBox::VALUE_MODES[m_ui.cbValueMode->currentIndex()].second, static_cast<IValueApplier&>(*this));
	}

	void Find(const QVariant& value, const int role) const
	{
		const auto setCurrentIndex = [this](const QModelIndex& index) {
			m_ui.treeView->setCurrentIndex(index);
		};
		const auto& model = *m_ui.treeView->model();
		if (const auto matched = model.match(model.index(0, 0), role, value, 1, (role == Role::Id ? Qt::MatchFlag::MatchExactly : Qt::MatchFlag::MatchStartsWith) | Qt::MatchFlag::MatchRecursive);
		    !matched.isEmpty())
			setCurrentIndex(matched.front());
		else if (role == Role::Id)
			setCurrentIndex(model.index(0, 0));

		if (const auto index = m_ui.treeView->currentIndex(); index.isValid())
			m_ui.treeView->scrollTo(index, QAbstractItemView::ScrollHint::PositionAtCenter);
	}

	void UpdateSectionSize() const
	{
		if (m_controller->GetItemType() != ItemType::Navigation)
			return;

		auto* header = m_ui.treeView->header();
		if (!header || !header->count())
			return;

		header->setSectionResizeMode(0, QHeaderView::Stretch);
		if (header->count() > 1 && m_controller->GetModeIndex() == static_cast<int>(NavigationMode::Authors))
			m_ui.treeView->UpdateSectionSize();
	}

	void OnCountChanged() const
	{
		m_countChangedTimer->start();
	}

	QString GetColumnSettingsKey(const char* value = nullptr, const QString& navigationModeName = {}) const
	{
		return QString("ui/%1/%2/Columns/%3%4")
		    .arg(m_controller->TrContext())
		    .arg(m_recentMode)
		    .arg(navigationModeName.isEmpty() ? m_navigationModeName : navigationModeName)
		    .arg(value ? QString("/%1").arg(value) : QString {});
	}

	QString GetValueModeKey() const
	{
		return QString(VALUE_MODE_KEY).arg(m_controller->TrContext());
	}

	QString GetRecentIdKey() const
	{
		auto key = QString("Collections/%1/%2%3/LastId").arg(m_collectionProvider->GetActiveCollection().id).arg(m_controller->TrContext()).arg(IsNavigation() ? QString("/%1").arg(m_recentMode) : QString {});

		return key;
	}

	void Filter(const int role, const std::function<std::unordered_set<QString>()>& getValues) const
	{
		auto* model = m_ui.treeView->model();
		if (!model)
			return;

		model->setData({}, QVariant::fromValue(getValues()), role);
		OnCountChanged();
		Find(m_currentId, Role::Id);
	}

private:
	TreeView&                                               m_self;
	PropagateConstPtr<ITreeViewController, std::shared_ptr> m_controller;
	std::shared_ptr<const ICollectionProvider>              m_collectionProvider;
	PropagateConstPtr<ISettings, std::shared_ptr>           m_settings;
	PropagateConstPtr<IUiFactory, std::shared_ptr>          m_uiFactory;
	PropagateConstPtr<IFilterProvider, std::shared_ptr>     m_filterProvider;
	PropagateConstPtr<ItemViewToolTipper, std::shared_ptr>  m_itemViewToolTipper;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> m_scrollBarController;
	PropagateConstPtr<ITreeViewDelegate, std::shared_ptr>   m_delegate;
	Ui::TreeView                                            m_ui {};
	QTimer                                                  m_filterTimer;
	QString                                                 m_navigationModeName;
	QString                                                 m_recentMode;
	QString                                                 m_currentId;
	std::shared_ptr<QMenu>                                  m_languageContextMenu;
	bool                                                    m_showRemoved { false };
	QString                                                 m_lastRestoredLayoutKey;
	ITreeViewController::RemoveItems                        m_removeItems;
	MenuEventFilter                                         m_menuEventFilter;
	HeaderView*                                             m_booksHeaderView;
	IDataItem::Flags                                        m_navigationItemFlags { IDataItem::Flags::None };
	const ArchiveSorter                                     m_archiveSorter;
	const QStringList                                       m_hiddenColumns;
	std::unique_ptr<QTimer>                                 m_countChangedTimer { Util::CreateUiTimer([this] {
        if (m_ui.treeView->model())
            m_ui.lblCount->setText(m_ui.treeView->model()->data({}, Role::Count).toString());
    }) };
};

TreeView::TreeView(
	const std::shared_ptr<const IDatabaseUser>& databaseUser,
	std::shared_ptr<const ICollectionProvider>  collectionProvider,
	std::shared_ptr<ISettings>                  settings,
	std::shared_ptr<IUiFactory>                 uiFactory,
	std::shared_ptr<IFilterProvider>            filterProvider,
	std::shared_ptr<ItemViewToolTipper>         itemViewToolTipper,
	std::shared_ptr<ScrollBarController>        scrollBarController,
	QWidget*                                    parent
)
	: QWidget(parent)
	, m_impl(*this, *databaseUser, std::move(collectionProvider), std::move(settings), std::move(uiFactory), std::move(filterProvider), std::move(itemViewToolTipper), std::move(scrollBarController))
{
	PLOGV << "TreeView created";
}

TreeView::~TreeView()
{
	PLOGV << "TreeView destroyed";
}

void TreeView::SetNavigationModeName(QString navigationModeName)
{
	m_impl->SetNavigationModeName(std::move(navigationModeName));
}

void TreeView::ShowRemoved(const bool showRemoved)
{
	m_impl->ShowRemoved(showRemoved);
}

QAbstractItemView* TreeView::GetView() const
{
	return m_impl->GetView();
}

void TreeView::SetMode(const int mode, const QString& id)
{
	m_impl->SetMode(mode, id);
}

void TreeView::OnBookTitleToSearchVisibleChanged() const
{
	m_impl->OnBookTitleToSearchVisibleChanged();
}

void TreeView::OnCurrentNavigationItemChanged(const QModelIndex& index)
{
	m_impl->OnCurrentNavigationItemChanged(index);
}

void TreeView::resizeEvent(QResizeEvent* event)
{
	m_impl->ResizeEvent(event);
	QWidget::resizeEvent(event);
}
