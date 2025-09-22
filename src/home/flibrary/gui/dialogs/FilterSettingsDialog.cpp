#include "ui_FilterSettingsDialog.h"

#include "FilterSettingsDialog.h"

#include <QSortFilterProxyModel>
#include <QTimer>

#include "fnd/algorithm.h"

#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"

#include "gutil/GeometryRestorable.h"
#include "gutil/util.h"
#include "util/localization.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

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
			CheckedOnly = Flibrary::Role::Last
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
		return role == Role::CheckedOnly ? Util::Set(checkedOnly, value.toBool(), [this] { invalidateFilter(); }) : QSortFilterProxyModel::setData(index, value, role);
	}

private: // QSortFilterProxyModel
	bool filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const override
	{
		if (!checkedOnly)
			return true;

		auto check = [&](const int column)
		{
			const auto sourceIndex = sourceModel->index(sourceRow, column, sourceParent);
			const auto checked = sourceIndex.data(Qt::CheckStateRole);
			return !checked.isValid() || checked.value<Qt::CheckState>() == Qt::Checked;
		};

		return check(1) || check(2);
	}
};

} // namespace

struct FilterSettingsDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
	, private ModeComboBox::IValueApplier
{
	QDialog& self;
	std::shared_ptr<const IModelProvider> modelProvider;
	PropagateConstPtr<ISettings, std::shared_ptr> settings;
	PropagateConstPtr<IFilterController, std::shared_ptr> filterController;
	PropagateConstPtr<IFilterDataProvider, std::shared_ptr> dataProvider;
	PropagateConstPtr<ItemViewToolTipper, std::shared_ptr> itemViewToolTipper;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> scrollBarController;
	PropagateConstPtr<QAbstractItemModel> model { std::unique_ptr<QAbstractItemModel> {} };
	const IFilterController::FilteredNavigation* filteredNavigation { nullptr };
	QTimer filterTimer;
	Ui::FilterSettingsDialog ui {};

	Impl(QDialog& self,
	     std::shared_ptr<const IModelProvider> modelProvider,
	     std::shared_ptr<ISettings> settings,
	     std::shared_ptr<IFilterController> filterController,
	     std::shared_ptr<IFilterDataProvider> dataProvider,
	     std::shared_ptr<ItemViewToolTipper> itemViewToolTipper,
	     std::shared_ptr<ScrollBarController> scrollBarController)
		: GeometryRestorable(*this, settings, "FilterSettingsDialog")
		, GeometryRestorableObserver(self)
		, self { self }
		, modelProvider { std::move(modelProvider) }
		, settings { std::move(settings) }
		, filterController { std::move(filterController) }
		, dataProvider { std::move(dataProvider) }
		, itemViewToolTipper { std::move(itemViewToolTipper) }
		, scrollBarController { std::move(scrollBarController) }
	{
		ui.setupUi(&self);
		Init();
	}

	~Impl() override
	{
		settings->Set(RECENT_TAB_KEY, ui.tabs->currentIndex());
		auto& header = *ui.view->header();
		Util::SaveHeaderSectionWidth(header, *this->settings, FIELD_WIDTH_KEY);
		SaveGeometry();
	}

private: //	ModeComboBox::IValueApplier
	void Find() override
	{
		Find(ui.value->text(), Qt::DisplayRole);
	}

	void Filter() override
	{
		filterTimer.start();
	}

private:
	void Init()
	{
		ui.checkBoxShowCheckedOnly->setChecked(settings->Get(SHOW_CHECKED_ONLY_KEY, ui.checkBoxShowCheckedOnly->isChecked()));

		std::vector<NavigationMode> indexToMode;
		for (size_t i = 0, sz = static_cast<size_t>(NavigationMode::Last); i < sz; ++i)
		{
			const auto& description = IFilterController::GetFilteredNavigationDescription(static_cast<NavigationMode>(i));
			if (!description.table)
				continue;

			auto* widget = new QWidget(&self);
			widget->setLayout(new QVBoxLayout);
			widget->layout()->setContentsMargins(0, 0, 0, 0);
			ui.tabs->addTab(widget, Loc::Tr(Loc::NAVIGATION, description.navigationTitle));
			indexToMode.emplace_back(description.navigationMode);
		}
		connect(ui.tabs, &QTabWidget::tabCloseRequested, [this](const int index) { OnTabCloseRequest(index); });
		connect(ui.tabs, &QTabWidget::currentChanged, [this, indexToMode = std::move(indexToMode)](const int tabIndex) { OnTabChanged(indexToMode, tabIndex); });
		connect(ui.checkBoxShowCheckedOnly, &QCheckBox::toggled, &self, [this] { OnShowCheckedOnlyChanged(); });
		connect(&filterTimer, &QTimer::timeout, &self, [&] { OnFilterTimerTimeout(); });

		dataProvider->SetNavigationRequestCallback([this](IDataItem::Ptr root) { OnSetNavigationRequestCallback(std::move(root)); });

		if (const auto index = settings->Get(RECENT_TAB_KEY, 0))
			ui.tabs->setCurrentIndex(index);
		else
			ui.tabs->currentChanged(index);

		connect(ui.btnCancel, &QAbstractButton::clicked, &self, &QDialog::reject);
		connect(ui.btnApply, &QAbstractButton::clicked, [this] { Apply(); });
		connect(ui.value, &QLineEdit::textChanged, &self, [&] { OnValueChanged(); });
		connect(ui.cbValueMode, &QComboBox::currentIndexChanged, &self, [&] { OnValueModeIndexChanged(); });

		scrollBarController->SetScrollArea(ui.view);
		ui.view->viewport()->installEventFilter(itemViewToolTipper.get());
		ui.view->viewport()->installEventFilter(scrollBarController.get());

		ui.checkBoxFilterEnabled->setChecked(filterController->IsFilterEnabled());

		if (const auto it = std::ranges::find_if(ModeComboBox::VALUE_MODES, [mode = settings->Get(RECENT_VALUE_MODE_KEY).toString()](const auto& item) { return mode == item.first; });
		    it != std::cend(ModeComboBox::VALUE_MODES))
			ui.cbValueMode->setCurrentIndex(static_cast<int>(std::distance(std::cbegin(ModeComboBox::VALUE_MODES), it)));

		filterTimer.setSingleShot(true);
		filterTimer.setInterval(std::chrono::milliseconds(200));

		LoadGeometry();
	}

	void OnSetNavigationRequestCallback(IDataItem::Ptr root)
	{
		assert(filteredNavigation);
		auto sourceMode = std::invoke(filteredNavigation->modelGetter, *modelProvider, std::move(root));
		model.reset(SortFilterProxyModel::Create(std::move(sourceMode)));
		ui.view->setModel(model.get());
		if (!model)
			return;

		model->setData({}, QVariant::fromValue(filteredNavigation->navigationMode), Role::NavigationMode);
		OnShowCheckedOnlyChangedImpl();
		Util::LoadHeaderSectionWidth(*ui.view->header(), *settings, FIELD_WIDTH_KEY);
	}

	void OnValueModeIndexChanged()
	{
		settings->Set(RECENT_VALUE_MODE_KEY, ui.cbValueMode->currentData());
		if (model)
			model->setData({}, {}, Role::TextFilter);
		OnValueChanged();
		ui.value->setFocus(Qt::FocusReason::OtherFocusReason);
	}

	void OnFilterTimerTimeout()
	{
		if (!model)
			return;

		auto currentId = [this]
		{
			const auto currentIndex = ui.view->currentIndex();
			return currentIndex.isValid() ? currentIndex.data(Role::Id) : QVariant {};
		}();
		model->setData({}, ui.value->text(), Role::TextFilter);
		if (!currentId.isValid())
			return Find(currentId, Role::Id);
		if (model->rowCount() != 0)
			ui.view->setCurrentIndex(model->index(0, 0));
	}

	void OnTabCloseRequest(const int index)
	{
		ui.tabs->widget(index)->layout()->removeWidget(ui.content);
		if (model)
			Util::SaveHeaderSectionWidth(*ui.view->header(), *this->settings, FIELD_WIDTH_KEY);
	}

	void OnTabChanged(const std::vector<NavigationMode>& indexToMode, const int tabIndex)
	{
		ui.view->setModel(nullptr);
		model.reset();
		const auto index = static_cast<size_t>(tabIndex);
		filteredNavigation = &IFilterController::GetFilteredNavigationDescription(indexToMode[index]);
		assert(filteredNavigation);
		dataProvider->SetNavigationMode(indexToMode[index]);
		dataProvider->RequestNavigation();
		ui.tabs->widget(tabIndex)->layout()->addWidget(ui.content);
		SetFont();
	}

	void OnShowCheckedOnlyChanged()
	{
		OnShowCheckedOnlyChangedImpl();
		settings->Set(SHOW_CHECKED_ONLY_KEY, ui.checkBoxShowCheckedOnly->isChecked());
	}

	void OnShowCheckedOnlyChangedImpl()
	{
		if (model)
			model->setData({}, ui.checkBoxShowCheckedOnly->isChecked(), SortFilterProxyModel::Role::CheckedOnly);
	}

	void OnValueChanged()
	{
		std::invoke(ModeComboBox::VALUE_MODES[ui.cbValueMode->currentIndex()].second, static_cast<IValueApplier&>(*this));
	}

	void Apply()
	{
		filterController->SetFilterEnabled(ui.checkBoxFilterEnabled->isChecked());
		filterController->Apply();
		self.accept();
	}

	void SetFont() const
	{
		for (auto* widget : self.findChildren<QWidget*>())
			widget->setFont(self.parentWidget()->font());
	}

	void Find(const QVariant& value, const int role) const
	{
		assert(model);
		const auto setCurrentIndex = [this](const QModelIndex& index) { ui.view->setCurrentIndex(index); };
		if (const auto matched = model->match(model->index(0, 0), role, value, 1, (role == Role::Id ? Qt::MatchFlag::MatchExactly : Qt::MatchFlag::MatchStartsWith) | Qt::MatchFlag::MatchRecursive);
		    !matched.isEmpty())
			setCurrentIndex(matched.front());
		else if (role == Role::Id)
			setCurrentIndex(model->index(0, 0));

		if (const auto index = ui.view->currentIndex(); index.isValid())
			ui.view->scrollTo(index, QAbstractItemView::ScrollHint::PositionAtCenter);
	}
	NON_COPY_MOVABLE(Impl)
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
