#include "ui_FilterSettingsWindow.h"

#include "FilterSettingsWindow.h"

#include <QMenu>

#include "interface/constants/Localization.h"
#include "interface/logic/IModel.h"

#include "gutil/GeometryRestorable.h"
#include "gutil/util.h"
#include "util/localization.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

using Role = ILanguageModel::Role;

constexpr auto FIELD_WIDTH_KEY = "ui/View/UniFilter/columnWidths";
constexpr auto SORT_INDEX_KEY = "ui/View/UniFilter/sortIndex";
constexpr auto SORT_ORDER_KEY = "ui/View/UniFilter/sortOrder";
constexpr auto RECENT_TAB_KEY = "ui/View/UniFilter/recentTab";

//std::unordered_set<QString> GetSelected(const QAbstractItemModel& model)
//{
//	auto list = model.data({}, Role::SelectedList).toStringList();
//	std::unordered_set<QString> selected;
//	std::ranges::move(list, std::inserter(selected, selected.end()));
//	return selected;
//}
//
//void SetModelData(QAbstractItemModel& model, const int role, const QVariant& value = {}, const QModelIndex& index = {})
//{
//	model.setData(index, value, role);
//}

}

struct FilterSettingsWindow::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	StackedPage& self;
	PropagateConstPtr<ISettings, std::shared_ptr> settings;
	PropagateConstPtr<IFilterController, std::shared_ptr> filterController;
	PropagateConstPtr<IFilterDataProvider, std::shared_ptr> dataProvider;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> scrollBarController;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> model { std::shared_ptr<QAbstractItemModel> {} };
	Ui::FilterSettingsWindow ui {};

	Impl(FilterSettingsWindow& self,
	     std::shared_ptr<ISettings> settings,
	     std::shared_ptr<IFilterController> filterController,
	     std::shared_ptr<IFilterDataProvider> dataProvider,
	     std::shared_ptr<ScrollBarController> scrollBarController)
		: GeometryRestorable(*this, settings, "FilterSettingsWindow")
		, GeometryRestorableObserver(self)
		, self { self }
		, settings { std::move(settings) }
		, filterController { std::move(filterController) }
		, dataProvider { std::move(dataProvider) }
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
		settings->Set(SORT_INDEX_KEY, header.sortIndicatorSection());
		settings->Set(SORT_ORDER_KEY, header.sortIndicatorOrder());
	}

private:
	void Init()
	{
		std::vector<NavigationMode> indexToMode;
		for (size_t i = 0, sz = static_cast<size_t>(NavigationMode::Last); i < sz; ++i)
		{
			const auto& description = IFilterController::GetFilteredNavigationDescription(i);
			if (!description.table)
				continue;

			auto* widget = new QWidget(&self);
			widget->setLayout(new QVBoxLayout);
			widget->layout()->setContentsMargins(0, 0, 0, 0);
			ui.tabs->addTab(widget, Loc::Tr(Loc::NAVIGATION, description.navigationTitle));
			indexToMode.emplace_back(description.navigationMode);
		}
		connect(ui.tabs, &QTabWidget::tabCloseRequested, [this](const int index) { ui.tabs->widget(index)->layout()->removeWidget(ui.view); });
		connect(ui.tabs,
		        &QTabWidget::currentChanged,
		        [this, indexToMode = std::move(indexToMode)](const int tabIndex)
		        {
					ui.view->setModel(nullptr);
					model.reset();
					const auto index = static_cast<size_t>(tabIndex);
					dataProvider->SetNavigationMode(indexToMode[index]);
					dataProvider->RequestNavigation();
					ui.tabs->widget(tabIndex)->layout()->addWidget(ui.view);
				});
		dataProvider->SetNavigationRequestCallback(
			[this](IDataItem::Ptr root)
			{
			});

		if (const auto index = settings->Get(RECENT_TAB_KEY, 0))
			ui.tabs->setCurrentIndex(index);
		else
			ui.tabs->currentChanged(index);

		connect(ui.btnCancel, &QAbstractButton::clicked, self.closeAction, &QAction::trigger);
		connect(ui.btnApply, &QAbstractButton::clicked, [this] { Apply(); });

		//		for (auto [action, role] : std::initializer_list<std::pair<QAction*, int>> {
		//				 {     ui.actionLanguageCheckAll,     Role::CheckAll },
		//				 {   ui.actionLanguageUncheckAll,   Role::UncheckAll },
		//				 { ui.actionLanguageInvertChecks, Role::RevertChecks },
		//        })
		//			connect(action, &QAction::triggered, &self, [=] { SetModelData(*model, role); });

		//		connect(ui.view, &QWidget::customContextMenuRequested, &self, [&] { OnLanguagesContextMenuRequested(); });
		//		connect(model,
		//		        &QAbstractItemModel::modelReset,
		//		        [this, model, filtered = this->languageFilterController->ToProvider().GetFilteredCodes()]() mutable
		//		        {
		//					if (filtered.empty())
		//						return;
		//
		//					const QStringList list { std::make_move_iterator(filtered.begin()), std::make_move_iterator(filtered.end()) };
		//					filtered.clear();
		//					model->setData({}, QVariant::fromValue(list), Role::SelectedList);
		//				});
		//		ui.view->setModel(model);
		scrollBarController->SetScrollArea(ui.view);
		ui.view->viewport()->installEventFilter(scrollBarController.get());

		ui.checkBoxFilterEnabled->setChecked(filterController->IsFilterEnabled());

		LoadGeometry();
		auto& header = *ui.view->header();
		Util::LoadHeaderSectionWidth(header, *settings, FIELD_WIDTH_KEY);
		header.setSortIndicator(settings->Get(SORT_INDEX_KEY, header.sortIndicatorSection()), settings->Get(SORT_ORDER_KEY, header.sortIndicatorOrder()));
	}

	void Apply()
	{
		filterController->SetFilterEnabled(ui.checkBoxFilterEnabled->isChecked());
		self.closeAction->trigger();
	}

	void OnLanguagesContextMenuRequested() const
	{
		QMenu menu;
		menu.setFont(self.font());
		menu.addAction(ui.actionLanguageCheckAll);
		menu.addAction(ui.actionLanguageUncheckAll);
		menu.addAction(ui.actionLanguageInvertChecks);
		menu.exec(QCursor::pos());
	}

	NON_COPY_MOVABLE(Impl)
};

FilterSettingsWindow::FilterSettingsWindow(const std::shared_ptr<const IParentWidgetProvider>& parentWidgetProvider,
                                           std::shared_ptr<ISettings> settings,
                                           std::shared_ptr<IFilterController> filterController,
                                           std::shared_ptr<IFilterDataProvider> dataProvider,
                                           std::shared_ptr<ScrollBarController> scrollBarController,
                                           QWidget* parent)
	: StackedPage(parentWidgetProvider->GetWidget(parent))
	, m_impl(*this, std::move(settings), std::move(filterController), std::move(dataProvider), std::move(scrollBarController))
{
	PLOGV << "FilterSettingsWindow created";
}

FilterSettingsWindow::~FilterSettingsWindow()
{
	PLOGV << "FilterSettingsWindow destroyed";
}
