#include "ui_FilterSettingsDialog.h"

#include "FilterSettingsDialog.h"

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

}

struct FilterSettingsDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	QDialog& self;
	std::shared_ptr<const IModelProvider> modelProvider;
	PropagateConstPtr<ISettings, std::shared_ptr> settings;
	PropagateConstPtr<IFilterController, std::shared_ptr> filterController;
	PropagateConstPtr<IFilterDataProvider, std::shared_ptr> dataProvider;
	PropagateConstPtr<ItemViewToolTipper, std::shared_ptr> itemViewToolTipper;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> scrollBarController;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> model { std::shared_ptr<QAbstractItemModel> {} };
	const IFilterController::FilteredNavigation* filteredNavigation { nullptr };
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

private:
	void Init()
	{
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
		connect(ui.tabs,
		        &QTabWidget::tabCloseRequested,
		        [this](const int index)
		        {
					ui.tabs->widget(index)->layout()->removeWidget(ui.view);
					if (model)
						Util::SaveHeaderSectionWidth(*ui.view->header(), *this->settings, FIELD_WIDTH_KEY);
				});
		connect(ui.tabs,
		        &QTabWidget::currentChanged,
		        [this, indexToMode = std::move(indexToMode)](const int tabIndex)
		        {
					ui.view->setModel(nullptr);
					model.reset();
					const auto index = static_cast<size_t>(tabIndex);
					filteredNavigation = &IFilterController::GetFilteredNavigationDescription(indexToMode[index]);
					assert(filteredNavigation);
					dataProvider->SetNavigationMode(indexToMode[index]);
					dataProvider->RequestNavigation();
					ui.tabs->widget(tabIndex)->layout()->addWidget(ui.view);
				});

		dataProvider->SetNavigationRequestCallback(
			[this](IDataItem::Ptr root)
			{
				assert(filteredNavigation);
				model.reset(std::invoke(filteredNavigation->modelGetter, *modelProvider, std::move(root)));
				ui.view->setModel(model.get());
				ui.view->setFont(self.font());
				ui.view->header()->setFont(self.font());
				if (!model)
					return;

				model->setData({}, QVariant::fromValue(filteredNavigation->navigationMode), Role::NavigationMode);
				Util::LoadHeaderSectionWidth(*ui.view->header(), *settings, FIELD_WIDTH_KEY);
			});

		if (const auto index = settings->Get(RECENT_TAB_KEY, 0))
			ui.tabs->setCurrentIndex(index);
		else
			ui.tabs->currentChanged(index);

		connect(ui.btnCancel, &QAbstractButton::clicked, &self, &QDialog::reject);
		connect(ui.btnApply, &QAbstractButton::clicked, [this] { Apply(); });

		scrollBarController->SetScrollArea(ui.view);
		ui.view->viewport()->installEventFilter(itemViewToolTipper.get());
		ui.view->viewport()->installEventFilter(scrollBarController.get());

		ui.checkBoxFilterEnabled->setChecked(filterController->IsFilterEnabled());

		LoadGeometry();
	}

	void Apply()
	{
		filterController->SetFilterEnabled(ui.checkBoxFilterEnabled->isChecked());
		filterController->Apply();
		self.accept();
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
