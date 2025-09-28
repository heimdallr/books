#include "ui_FilterSettingsDialog.h"

#include "FilterSettingsDialog.h"

#include <QMenu>
#include <QSortFilterProxyModel>
#include <QTimer>

#include "fnd/algorithm.h"

#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"
#include "interface/logic/IDataItem.h"

#include "gutil/GeometryRestorable.h"
#include "gutil/util.h"
#include "util/FunctorExecutionForwarder.h"
#include "util/localization.h"

#include "log.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

constexpr auto FIELD_WIDTH_KEY = "ui/View/UniFilter/columnWidths";
constexpr auto RECENT_TAB_KEY = "ui/View/UniFilter/recentTab";
constexpr auto RECENT_VALUE_MODE_KEY = "ui/View/UniFilter/valueMode";
constexpr auto SHOW_CHECKED_ONLY_KEY = "ui/View/UniFilter/showCheckedOnly";

struct SortFilterProxyModel final : QSortFilterProxyModel
{
	struct Role
	{
		enum
		{
			CheckedOnly = Flibrary::Role::Last,
			FilterDataChanged,
		};
	};

	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> sourceModel;
	bool checkedOnly { false };

	static std::unique_ptr<QAbstractItemModel> Create(std::shared_ptr<QAbstractItemModel> sourceModel)
	{
		return std::make_unique<SortFilterProxyModel>(std::move(sourceModel));
	}

	explicit SortFilterProxyModel(std::shared_ptr<QAbstractItemModel> sourceModel)
		: sourceModel { std::move(sourceModel) }
	{
		QSortFilterProxyModel::setSourceModel(this->sourceModel.get());
	}

private:
	bool setData(const QModelIndex& index, const QVariant& value, const int role) override
	{
		switch (role)
		{
			case Role::CheckedOnly:
				return Util::Set(checkedOnly, value.toBool(), [this] { invalidateFilter(); });

			case Role::FilterDataChanged:
				return invalidateFilter(), true;

			default:
				break;
		}

		return QSortFilterProxyModel::setData(index, value, role);
	}

private: // QSortFilterProxyModel
	bool filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const override
	{
		if (!checkedOnly)
			return true;

		auto check = [&](const int column)
		{
			const auto sourceIndex = sourceModel->index(sourceRow, column, sourceParent);
			[[maybe_unused]] const auto id = sourceIndex.data(Flibrary::Role::Id);
			const auto checked = sourceIndex.data(Qt::CheckStateRole);
			if (checked.isValid())
				return checked.value<Qt::CheckState>() == Qt::Checked;

			for (int row = 0, count = sourceModel->rowCount(sourceIndex); row < count; ++row)
				if (filterAcceptsRow(row, sourceIndex))
					return true;
			return false;
		};

		return check(1) || check(2);
	}
};

} // namespace

class FilterSettingsDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
	, ModeComboBox::IValueApplier
	, public IFilterController::ICallback
	, public std::enable_shared_from_this<Impl>
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(QDialog& self,
	     std::shared_ptr<const IModelProvider> modelProvider,
	     std::shared_ptr<ISettings> settings,
	     std::shared_ptr<IFilterController> filterController,
	     std::shared_ptr<IFilterDataProvider> dataProvider,
	     std::shared_ptr<ItemViewToolTipper> itemViewToolTipper,
	     std::shared_ptr<ScrollBarController> scrollBarController)
		: GeometryRestorable(*this, settings, "FilterSettingsDialog")
		, GeometryRestorableObserver(self)
		, m_self { self }
		, m_modelProvider { std::move(modelProvider) }
		, m_settings { std::move(settings) }
		, m_filterController { std::move(filterController) }
		, m_dataProvider { std::move(dataProvider) }
		, m_itemViewToolTipper { std::move(itemViewToolTipper) }
		, m_scrollBarController { std::move(scrollBarController) }
	{
		m_ui.setupUi(&self);
		Init();
	}

	~Impl() override
	{
		m_settings->Set(RECENT_TAB_KEY, m_ui.tabs->currentIndex());
		auto& header = *m_ui.view->header();
		Util::SaveHeaderSectionWidth(header, *this->m_settings, FIELD_WIDTH_KEY);
		SaveGeometry();
	}

private: //	ModeComboBox::IValueApplier
	void Find() override
	{
		FindImpl(m_ui.value->text(), Qt::DisplayRole);
	}

	void Filter() override
	{
		m_filterTimer.start();
	}

private: // IFilterController::ICallback
	void OnStarted() override
	{
		SetHideFilteredStarted(true);
	}

	void OnFinished() override
	{
		SetHideFilteredStarted(false);
	}

	bool Stopped() const override
	{
		return !m_hideFilteredStarted;
	}

	std::weak_ptr<ICallback> Ptr() noexcept override
	{
		return weak_from_this();
	}

private:
	void Init()
	{
		m_ui.checkBoxFilterEnabled->setChecked(m_filterController->IsFilterEnabled());
		m_ui.checkBoxShowCheckedOnly->setChecked(m_settings->Get(SHOW_CHECKED_ONLY_KEY, m_ui.checkBoxShowCheckedOnly->isChecked()));
		m_dataProvider->SetNavigationRequestCallback([this](IDataItem::Ptr root) { OnSetNavigationRequestCallback(std::move(root)); });
		m_filterTimer.setSingleShot(true);
		m_filterTimer.setInterval(std::chrono::milliseconds(200));
		m_scrollBarController->SetScrollArea(m_ui.view);
		m_ui.view->viewport()->installEventFilter(m_itemViewToolTipper.get());
		m_ui.view->viewport()->installEventFilter(m_scrollBarController.get());
		m_ui.view->header()->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

		connect(&m_filterTimer, &QTimer::timeout, &m_self, [&] { FilterImpl(); });
		connect(m_ui.tabs, &QTabWidget::currentChanged, [this, indexToMode = CreateTabs()](const int tabIndex) { OnTabChanged(indexToMode, tabIndex); });
		connect(m_ui.tabs, &QTabWidget::tabCloseRequested, [this](const int index) { OnTabCloseRequest(index); });
		connect(m_ui.checkBoxShowCheckedOnly, &QCheckBox::toggled, &m_self, [this] { OnShowCheckedOnlyChanged(); });
		connect(m_ui.btnCancel, &QAbstractButton::clicked, &m_self, &QDialog::reject);
		connect(m_ui.btnApply, &QAbstractButton::clicked, [this] { Apply(); });
		connect(m_ui.value, &QLineEdit::textChanged, &m_self, [&] { OnValueChanged(); });
		connect(m_ui.cbValueMode, &QComboBox::currentIndexChanged, &m_self, [&] { OnValueModeIndexChanged(); });
		connect(m_ui.view, &QWidget::customContextMenuRequested, &m_self, [this] { OnViewContextMenuRequested(); });
		connect(m_ui.view->header(), &QWidget::customContextMenuRequested, &m_self, [this](const QPoint& pos) { OnHeaderViewContextMenuRequested(pos); });
		connect(m_ui.btnStop, &QAbstractButton::clicked, &m_self, [this] { m_hideFilteredStarted = false; });

		if (const auto index = m_settings->Get(RECENT_TAB_KEY, 0))
			m_ui.tabs->setCurrentIndex(index);
		else
			m_ui.tabs->currentChanged(index);

		if (const auto it = std::ranges::find_if(ModeComboBox::VALUE_MODES, [mode = m_settings->Get(RECENT_VALUE_MODE_KEY).toString()](const auto& item) { return mode == item.first; });
		    it != std::cend(ModeComboBox::VALUE_MODES))
			m_ui.cbValueMode->setCurrentIndex(static_cast<int>(std::distance(std::cbegin(ModeComboBox::VALUE_MODES), it)));

		LoadGeometry();
	}

	std::vector<NavigationMode> CreateTabs() const
	{
		std::vector<NavigationMode> indexToMode;

		auto range = IFilterProvider::GetDescriptions();
		std::ranges::transform(range,
		                       std::back_inserter(indexToMode),
		                       [this](const auto& description)
		                       {
								   auto* widget = new QWidget(&m_self);
								   widget->setLayout(new QVBoxLayout);
								   widget->layout()->setContentsMargins(0, 0, 0, 0);
								   m_ui.tabs->addTab(widget, Loc::Tr(Loc::NAVIGATION, description.navigationTitle));
								   return description.navigationMode;
							   });

		return indexToMode;
	}

	void OnTabChanged(const std::vector<NavigationMode>& indexToMode, const int tabIndex)
	{
		m_ui.view->setModel(nullptr);
		m_model.reset();
		const auto index = static_cast<size_t>(tabIndex);
		m_filteredNavigation = &IFilterController::GetFilteredNavigationDescription(indexToMode[index]);
		assert(m_filteredNavigation);
		m_dataProvider->SetNavigationMode(indexToMode[index]);
		m_dataProvider->RequestNavigation();
		m_ui.tabs->widget(tabIndex)->layout()->addWidget(m_ui.content);
		SetFont();
	}

	void OnTabCloseRequest(const int index)
	{
		m_ui.tabs->widget(index)->layout()->removeWidget(m_ui.content);
		if (m_model)
			Util::SaveHeaderSectionWidth(*m_ui.view->header(), *this->m_settings, FIELD_WIDTH_KEY);
	}

	void OnSetNavigationRequestCallback(IDataItem::Ptr root)
	{
		assert(m_filteredNavigation);
		auto sourceMode = std::invoke(m_filteredNavigation->modelGetter, *m_modelProvider, std::move(root));
		m_model.reset(SortFilterProxyModel::Create(std::move(sourceMode)));
		m_ui.view->setModel(m_model.get());
		if (!m_model)
			return;

		m_model->setData({}, QVariant::fromValue(m_filteredNavigation->navigationMode), Role::NavigationMode);
		OnShowCheckedOnlyChangedImpl();
		Util::LoadHeaderSectionWidth(*m_ui.view->header(), *m_settings, FIELD_WIDTH_KEY);
	}

	void OnValueChanged()
	{
		std::invoke(ModeComboBox::VALUE_MODES[m_ui.cbValueMode->currentIndex()].second, static_cast<IValueApplier&>(*this));
	}

	void OnValueModeIndexChanged()
	{
		m_settings->Set(RECENT_VALUE_MODE_KEY, m_ui.cbValueMode->currentData());
		if (m_model)
			m_model->setData({}, {}, Role::TextFilter);
		OnValueChanged();
		m_ui.value->setFocus(Qt::FocusReason::OtherFocusReason);
	}

	void FilterImpl()
	{
		if (!m_model)
			return;

		auto currentId = [this]
		{
			const auto currentIndex = m_ui.view->currentIndex();
			return currentIndex.isValid() ? currentIndex.data(Role::Id) : QVariant {};
		}();
		m_model->setData({}, m_ui.value->text(), Role::TextFilter);
		if (!currentId.isValid())
			return FindImpl(currentId, Role::Id);
		if (m_model->rowCount() != 0)
			m_ui.view->setCurrentIndex(m_model->index(0, 0));
	}

	void FindImpl(const QVariant& value, const int role) const
	{
		assert(m_model);
		const auto setCurrentIndex = [this](const QModelIndex& index) { m_ui.view->setCurrentIndex(index); };
		if (const auto matched = m_model->match(m_model->index(0, 0), role, value, 1, (role == Role::Id ? Qt::MatchFlag::MatchExactly : Qt::MatchFlag::MatchStartsWith) | Qt::MatchFlag::MatchRecursive);
		    !matched.isEmpty())
			setCurrentIndex(matched.front());
		else if (role == Role::Id)
			setCurrentIndex(m_model->index(0, 0));

		if (const auto index = m_ui.view->currentIndex(); index.isValid())
			m_ui.view->scrollTo(index, QAbstractItemView::ScrollHint::PositionAtCenter);
	}

	void OnViewContextMenuRequested() const
	{
		if (m_filteredNavigation->navigationMode != NavigationMode::Genres)
			return;

		const auto has = [this](const bool value)
		{
			for (int row = 0, count = m_model->rowCount(); row < count; ++row)
				if (m_ui.view->isExpanded(m_model->index(row, 0)) == value)
					return true;
			return false;
		};

		QMenu menu;
		menu.setFont(m_self.font());
		menu.addAction(Loc::Tr(Loc::CONTEXT_MENU, Loc::TREE_COLLAPSE_ALL), [this] { m_ui.view->collapseAll(); })->setEnabled(has(true));
		menu.addAction(Loc::Tr(Loc::CONTEXT_MENU, Loc::TREE_EXPAND_ALL), [this] { m_ui.view->expandAll(); })->setEnabled(has(false));
		menu.exec(QCursor::pos());
	}

	void OnHeaderViewContextMenuRequested(const QPoint& pos)
	{
		if (m_sectionClicked = m_ui.view->header()->logicalIndexAt(pos); m_sectionClicked < 1)
			return;

		QMenu menu;
		menu.setFont(m_self.font());
		menu.addAction(Loc::Tr(Loc::CONTEXT_MENU, Loc::CHECK_ALL), [this] { OnCheckActionTriggered([](IDataItem::Flags& dst, const IDataItem::Flags src) { dst |= src; }); });
		menu.addAction(Loc::Tr(Loc::CONTEXT_MENU, Loc::UNCHECK_ALL), [this] { OnCheckActionTriggered([](IDataItem::Flags& dst, const IDataItem::Flags src) { dst &= ~src; }); });
		menu.addAction(Loc::Tr(Loc::CONTEXT_MENU, Loc::INVERT_CHECK), [this] { OnCheckActionTriggered([](IDataItem::Flags& dst, const IDataItem::Flags src) { dst ^= src; }); });
		if (m_sectionClicked == 1)
			if (auto* action = menu.addAction(tr("Hide items with all books are filtered"), [this] { m_model->setData({}, QVariant::fromValue<ICallback*>(this), Role::HideFiltered); }); m_hideFilteredStarted)
				action->setEnabled(false);

		menu.exec(QCursor::pos());
	}

	void OnCheckActionTriggered(const std::function<void(IDataItem::Flags& dst, IDataItem::Flags src)>& operation)
	{
		const auto enumerate = [this](const QModelIndex& index, const auto& skip, const auto& functor, const auto& recursion) -> void
		{
			if (skip(index))
				return;

			for (int row = 0, rowCount = m_model->rowCount(index); row < rowCount; ++row)
			{
				const auto child = m_model->index(row, 0, index);
				functor(child);
				recursion(child, skip, functor, recursion);
			}
		};

		const auto flags = m_sectionClicked == 1 ? IDataItem::Flags::Filtered : m_sectionClicked == 2 ? IDataItem::Flags::BooksFiltered : (assert(false && "unexpected section"), IDataItem::Flags::None);
		const auto skip = [this](const QModelIndex& index) { return m_filteredNavigation->navigationMode == NavigationMode::Genres && index.isValid() && !m_ui.view->isExpanded(index); };
		const auto process = [&](const QModelIndex& index)
		{
			if (index.data(Role::ChildCount).toInt() > 0)
				return;

			auto itemFlag = index.data(Role::Flags).value<IDataItem::Flags>();
			operation(itemFlag, flags);
			m_model->setData(index, QVariant::fromValue(itemFlag), Role::Flags);
		};

		enumerate({}, skip, process, enumerate);

		if (m_ui.checkBoxShowCheckedOnly)
			m_model->setData({}, {}, SortFilterProxyModel::Role::FilterDataChanged);
	}

	void OnShowCheckedOnlyChanged()
	{
		OnShowCheckedOnlyChangedImpl();
		m_settings->Set(SHOW_CHECKED_ONLY_KEY, m_ui.checkBoxShowCheckedOnly->isChecked());
	}

	void OnShowCheckedOnlyChangedImpl()
	{
		if (!m_model)
			return;

		const auto id = [this]
		{
			const auto index = m_ui.view->currentIndex();
			return index.isValid() ? index.data(Role::Id) : QVariant {};
		}();
		m_model->setData({}, m_ui.checkBoxShowCheckedOnly->isChecked(), SortFilterProxyModel::Role::CheckedOnly);
		if (id.isValid())
			FindImpl(id, Role::Id);
	}

	void Apply()
	{
		m_filterController->SetFilterEnabled(m_ui.checkBoxFilterEnabled->isChecked());
		m_filterController->Apply();
		m_self.accept();
	}

	void SetFont() const
	{
		for (auto* widget : m_self.findChildren<QWidget*>())
			widget->setFont(m_self.parentWidget()->font());
	}

	void SetHideFilteredStarted(const bool started)
	{
		m_hideFilteredStarted = started;
		m_forwarder.Forward(
			[this]
			{
				m_ui.applyWidget->setVisible(!m_hideFilteredStarted);
				m_ui.progressBarWidget->setVisible(m_hideFilteredStarted);
			});
	}

private:
	QDialog& m_self;
	std::shared_ptr<const IModelProvider> m_modelProvider;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<IFilterController, std::shared_ptr> m_filterController;
	PropagateConstPtr<IFilterDataProvider, std::shared_ptr> m_dataProvider;
	PropagateConstPtr<ItemViewToolTipper, std::shared_ptr> m_itemViewToolTipper;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> m_scrollBarController;
	PropagateConstPtr<QAbstractItemModel> m_model { std::unique_ptr<QAbstractItemModel> {} };
	const IFilterController::FilteredNavigation* m_filteredNavigation { nullptr };
	QTimer m_filterTimer;
	int m_sectionClicked { -1 };

	std::atomic_bool m_hideFilteredStarted { false };
	Util::FunctorExecutionForwarder m_forwarder;

	Ui::FilterSettingsDialog m_ui {};
};

FilterSettingsDialog::FilterSettingsDialog(const std::shared_ptr<const IParentWidgetProvider>& parentWidgetProvider,
                                           std::shared_ptr<const IModelProvider> modelProvider,
                                           std::shared_ptr<ISettings> settings,
                                           std::shared_ptr<IFilterController> filterController,
                                           std::shared_ptr<IFilterDataProvider> dataProvider,
                                           std::shared_ptr<ItemViewToolTipper> itemViewToolTipper,
                                           std::shared_ptr<ScrollBarController> scrollBarController,
                                           QWidget* parent)
	: QDialog(parentWidgetProvider->GetWidget(parent))
	, m_impl(*this, std::move(modelProvider), std::move(settings), std::move(filterController), std::move(dataProvider), std::move(itemViewToolTipper), std::move(scrollBarController))
{
	PLOGV << "FilterSettingsDialog created";
}

FilterSettingsDialog::~FilterSettingsDialog()
{
	PLOGV << "FilterSettingsDialog destroyed";
}
