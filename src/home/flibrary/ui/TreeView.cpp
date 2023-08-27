#include "ui_TreeView.h"
#include "TreeView.h"

#include <ranges>
#include <stack>

#include <QActionGroup>
#include <QMenu>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>

#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"
#include "interface/logic/ITreeViewController.h"
#include "interface/ui/IUiFactory.h"

#include "util/ISettings.h"

#include "ItemViewToolTipper.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto VALUE_MODE_ICON_TEMPLATE = ":/icons/%1.png";
constexpr auto VALUE_MODE_KEY = "ui/%1/ValueMode";
constexpr auto VALUE_MODE_STYLE_SHEET = "QComboBox::drop-down {border-width: 0px;} QComboBox::down-arrow {image: url(noimg); border-width: 0px;}";
constexpr auto COLUMNS_KEY = "ui/%1/%2/Columns/%3";
constexpr auto COLUMN_WIDTH_LOCAL_KEY = "%1/Width";
constexpr auto COLUMN_INDEX_LOCAL_KEY = "%1/Index";
constexpr auto COLUMN_HIDDEN_LOCAL_KEY = "%1/Hidden";
constexpr auto SORT_INDICATOR_COLUMN_KEY = "Sort/Index";
constexpr auto SORT_INDICATOR_ORDER_KEY = "Sort/Order";
constexpr auto RECENT_LANG_FILTER_KEY = "ui/language";
constexpr auto RECENT_ID_KEY = "ui/%1";

class IValueApplier  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IValueApplier() = default;
	virtual void Find() = 0;
	virtual void Filter() = 0;
};

using ApplyValue = void(IValueApplier::*)();

constexpr std::pair<const char *, ApplyValue> VALUE_MODES[]
{
	{ QT_TRANSLATE_NOOP("TreeView", "Find"), &IValueApplier::Find },
	{ QT_TRANSLATE_NOOP("TreeView", "Filter"), &IValueApplier::Filter },
};

}

class TreeView::Impl final
	: ITreeViewController::IObserver
	, IValueApplier
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(TreeView & self
		, std::shared_ptr<ITreeViewController> controller
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<IUiFactory> uiFactory
		, std::shared_ptr<ItemViewToolTipper> itemViewToolTipper
	)
		: m_self(self)
		, m_controller(std::move(controller))
		, m_settings(std::move(settings))
		, m_uiFactory(std::move(uiFactory))
		, m_itemViewToolTipper(std::move(itemViewToolTipper))
	{
		Setup();
	}

	~Impl() override
	{
		SaveHeaderLayout();
		m_controller->UnregisterObserver(this);
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

	void ResizeEvent(const QResizeEvent * event)
	{
		if (m_controller->GetItemType() != ItemType::Books || m_recentMode.isEmpty() || m_navigationModeName.isEmpty())
			return;

		m_ui.treeView->header()->resizeSection(0, m_ui.treeView->header()->sectionSize(0) + (event->size().width() - event->oldSize().width()));
	}

private: // ITreeViewController::IObserver
	void OnModeChanged(const int index) override
	{
		m_ui.cbMode->setCurrentIndex(index);
	}

	void OnModelChanged(QAbstractItemModel * model) override
	{
		if (m_ui.treeView->model() != model)
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
			}

			if (model->rowCount() == 0)
				m_controller->SetCurrentId({});
		}

		RestoreHeaderLayout();
		OnCountChanged();
		OnValueChanged();
	}

	void OnContextMenuReady(const QString & id, const IDataItem::Ptr & item) override
	{
		if (m_ui.treeView->currentIndex().data(Role::Id).toString() != id)
			return;

		QMenu menu;
		std::stack<std::pair<IDataItem::Ptr, QMenu *>> stack { {{item, &menu}} };
		while (!stack.empty())
		{
			auto [parent, subMenu] = std::move(stack.top());
			stack.pop();
			for (size_t i = 0, sz = parent->GetChildCount(); i < sz; ++i)
			{
				auto child = parent->GetChild(i);
				const auto enabledStr = child->GetData(MenuItem::Column::Enabled);
				const auto enabled = enabledStr.isEmpty() || QVariant(enabledStr).toBool();
				const auto title = child->GetData().toStdString();

				if (child->GetChildCount() != 0)
				{
					auto * subSubMenu = stack.emplace(child, subMenu->addMenu(Loc::Tr(m_controller->TrContext(), title.data()))).second;
					subSubMenu->setEnabled(enabled);
					continue;
				}

				const auto childId = child->GetData(MenuItem::Column::Id).toInt();
				if (childId < 0)
				{
					subMenu->addSeparator();
					continue;
				}

				auto * action = subMenu->addAction(Loc::Tr(m_controller->TrContext(), title.data()), [&, child = std::move(child)]() mutable
				{
					m_controller->OnContextMenuTriggered(m_ui.treeView->model(), m_ui.treeView->currentIndex(), m_ui.treeView->selectionModel()->selectedIndexes(), std::move(child));
				});

				action->setEnabled(enabled);
			}
		}

		menu.exec(QCursor::pos());
	}

private: //	IValueApplier
	void Find() override
	{
		if (!m_ui.value->text().isEmpty())
			return Find(m_ui.value->text());

		if (!m_currentId.isEmpty())
			return Find(m_currentId, Role::Id);

		m_ui.treeView->setCurrentIndex(m_ui.treeView->model()->index(0, 0));
	}

	void Filter() override
	{
		m_filterTimer.start();
	}

private:
	void Setup()
	{
		m_ui.setupUi(&m_self);
		m_ui.treeView->setHeaderHidden(m_controller->GetItemType() == ItemType::Navigation);
		m_ui.treeView->viewport()->installEventFilter(m_itemViewToolTipper.get());

		if (m_controller->GetItemType() == ItemType::Books)
		{
			m_delegate = m_uiFactory->CreateTreeViewDelegateBooks(*m_ui.treeView);
			m_ui.treeView->setItemDelegate(m_delegate.get());
			m_ui.treeView->setSortingEnabled(true);

			auto * widget = m_ui.treeView->header();
			widget->setStretchLastSection(false);
			widget->setContextMenuPolicy(Qt::CustomContextMenu);
			connect(widget, &QWidget::customContextMenuRequested, &m_self, [&] (const QPoint & pos)
			{
				CreateHeaderContextMenu(pos);
			});
		}

		m_filterTimer.setSingleShot(true);
		m_filterTimer.setInterval(std::chrono::milliseconds(200));

		FillComboBoxes();
		Connect();

		OnModeChanged(m_controller->GetModeIndex());
		m_controller->RegisterObserver(this);

		QTimer::singleShot(0, [&]
		{
			emit m_self.NavigationModeNameChanged(m_ui.cbMode->currentData().toString());
		});
	}

	void FillComboBoxes()
	{
		m_ui.cbValueMode->setStyleSheet(VALUE_MODE_STYLE_SHEET);
		for (const auto * name : VALUE_MODES | std::views::keys)
			m_ui.cbValueMode->addItem(QIcon(QString(VALUE_MODE_ICON_TEMPLATE).arg(name)), "", QString(name));

		for (const auto * name : m_controller->GetModeNames())
			m_ui.cbMode->addItem(Loc::Tr(m_controller->TrContext(), name), QString(name));

		const auto it = std::ranges::find_if(VALUE_MODES, [mode = m_settings->Get(GetValueModeKey()).toString()] (const auto & item)
		{
			return mode == item.first;
		});
		if (it != std::cend(VALUE_MODES))
			m_ui.cbValueMode->setCurrentIndex(static_cast<int>(std::distance(std::cbegin(VALUE_MODES), it)));

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
			m_ui.treeView->model()->setData({}, m_ui.value->text(), Role::TextFilter);
			OnCountChanged();
			if (!m_currentId.isEmpty())
				return Find(m_currentId, Role::Id);
			if (m_ui.treeView->model()->rowCount() != 0 && !m_ui.treeView->currentIndex().isValid())
				m_ui.treeView->setCurrentIndex(m_ui.treeView->model()->index(0, 0));
		});
		connect(m_ui.treeView, &QWidget::customContextMenuRequested, &m_self, [&]
		{
			m_controller->RequestContextMenu(m_ui.treeView->currentIndex());
		});
		connect(m_ui.treeView, &QTreeView::doubleClicked, &m_self, [&]
		{
			m_controller->OnDoubleClicked(m_ui.treeView->currentIndex());
		});
	}

	void SaveHeaderLayout()
	{
		if (m_recentMode.isEmpty())
			return;

		if (const auto currentIndex = m_ui.treeView->currentIndex(); currentIndex.isValid())
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
		if (m_recentMode.isEmpty())
			return;

		m_currentId = m_settings->Get(GetRecentIdKey(), m_currentId);

		if (m_controller->GetItemType() != ItemType::Books || m_navigationModeName.isEmpty())
			return;

		auto * header = m_ui.treeView->header();
		std::map<int, int> widths;
		std::map<int, int> indices;
		auto sortIndex = 0;
		auto sortOrder = Qt::AscendingOrder;
		{
			SettingsGroup guard(*m_settings, GetColumnSettingsKey());

			for (const auto & column : m_settings->GetGroups())
			{
				bool ok = false;
				if (const auto logicalIndex = column.toInt(&ok); ok)
				{
					widths.emplace(logicalIndex, m_settings->Get(QString(COLUMN_WIDTH_LOCAL_KEY).arg(column), -1));
					indices.emplace(m_settings->Get(QString(COLUMN_INDEX_LOCAL_KEY).arg(column), -1), logicalIndex);
					m_settings->Get(QString(COLUMN_HIDDEN_LOCAL_KEY).arg(column), false)
						? header->hideSection(logicalIndex)
						: header->showSection(logicalIndex);
				}
			}

			sortIndex = m_settings->Get(SORT_INDICATOR_COLUMN_KEY, sortIndex);
			sortOrder = m_settings->Get(SORT_INDICATOR_ORDER_KEY, sortOrder);
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
			auto * action = menu->addAction(model->headerData(index, Qt::Horizontal).toString(), &m_self, [=, this_ = this] (const bool checked)
			{
				if (!checked)
					header->resizeSection(0, header->sectionSize(0) + header->sectionSize(index));
				header->setSectionHidden(index, !checked);
				if (checked)
					header->resizeSection(0, header->sectionSize(0) - header->sectionSize(index));

				this_->SaveHeaderLayout();
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

	void OnValueChanged()
	{
		((*this).*VALUE_MODES[m_ui.cbValueMode->currentIndex()].second)();
	}

	void Find(const QVariant & value, const int role = Qt::DisplayRole) const
	{
		if (const auto matched = m_ui.treeView->model()->match(m_ui.treeView->model()->index(0, 0), role, value, 1, (role == Role::Id ? Qt::MatchFlag::MatchExactly : Qt::MatchFlag::MatchStartsWith) | Qt::MatchFlag::MatchRecursive); !matched.isEmpty())
			m_ui.treeView->setCurrentIndex(matched.front());
		else if (role == Role::Id)
			m_ui.treeView->setCurrentIndex(m_ui.treeView->model()->index(0, 0));

		if (const auto index = m_ui.treeView->currentIndex(); index.isValid())
			m_ui.treeView->scrollTo(index, QAbstractItemView::ScrollHint::PositionAtCenter);
	}

	void OnCountChanged() const
	{
		m_ui.lblCount->setText(m_ui.treeView->model()->data({}, Role::Count).toString());
	}

	QString GetColumnSettingsKey(const char * value = nullptr) const
	{
		auto key = QString(COLUMNS_KEY)
			.arg(m_controller->TrContext())
			.arg(m_recentMode)
			.arg(m_navigationModeName)
			;

		if (value)
			key.append(QString("/%1").arg(value));

		return key;
	}

	QString GetValueModeKey() const
	{
		return QString(VALUE_MODE_KEY).arg(m_controller->TrContext());
	}

	QString GetRecentIdKey() const
	{
		auto key = QString(RECENT_ID_KEY).arg(m_controller->TrContext());

		if (m_controller->GetItemType() == ItemType::Navigation)
			key.append(QString("/%1").arg(m_recentMode));

		key.append("/LastId");

		return key;
	}

private:
	TreeView & m_self;
	PropagateConstPtr<ITreeViewController, std::shared_ptr> m_controller;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<IUiFactory, std::shared_ptr> m_uiFactory;
	PropagateConstPtr<ItemViewToolTipper, std::shared_ptr> m_itemViewToolTipper;
	Ui::TreeView m_ui {};
	QTimer m_filterTimer;
	QString m_navigationModeName;
	QString m_recentMode;
	QString m_currentId;
	std::shared_ptr<QMenu> m_languageContextMenu;
	std::shared_ptr<QAbstractItemDelegate> m_delegate;
	bool m_showRemoved { false };
};

TreeView::TreeView(std::shared_ptr<ITreeViewController> controller
	, std::shared_ptr<ISettings> settings
	, std::shared_ptr<IUiFactory> uiFactory
	, std::shared_ptr<ItemViewToolTipper> itemViewToolTipper
	, QWidget * parent
)
	: QWidget(parent)
	, m_impl(*this
		, std::move(controller)
		, std::move(settings)
		, std::move(uiFactory)
		, std::move(itemViewToolTipper)
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

void TreeView::resizeEvent(QResizeEvent * event)
{
	m_impl->ResizeEvent(event);
	QWidget::resizeEvent(event);
}