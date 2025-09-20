#include "ui_FilterDialog.h"

#include "FilterDialog.h"

#include <QMenu>

#include "interface/logic/IModel.h"

#include "gutil/GeometryRestorable.h"
#include "gutil/util.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

using Role = ILanguageModel::Role;

constexpr auto FIELD_WIDTH_KEY = "ui/Books/LanguageFilter/columnWidths";
constexpr auto SORT_INDEX_KEY = "ui/Books/LanguageFilter/sortIndex";
constexpr auto SORT_ORDER_KEY = "ui/Books/LanguageFilter/sortOrder";

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

struct FilterDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	QWidget& self;
	PropagateConstPtr<ISettings, std::shared_ptr> settings;
	PropagateConstPtr<IFilterController, std::shared_ptr> filterController;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> scrollBarController;
	Ui::FilterDialog ui {};

	Impl(FilterDialog& self, std::shared_ptr<ISettings> settings, std::shared_ptr<IFilterController> filterController, std::shared_ptr<ScrollBarController> scrollBarController)
		: GeometryRestorable(*this, settings, "FilterDialog")
		, GeometryRestorableObserver(self)
		, self { self }
		, settings { std::move(settings) }
		, filterController { std::move(filterController) }
		, scrollBarController { std::move(scrollBarController) }
	{
		ui.setupUi(&self);

		connect(&self, &QDialog::accepted, [this] { this->filterController->SetFilterEnabled(ui.checkBoxFilterEnabled->isChecked()); });

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
		this->scrollBarController->SetScrollArea(ui.view);
		ui.view->viewport()->installEventFilter(this->scrollBarController.get());

		ui.checkBoxFilterEnabled->setChecked(this->filterController->IsFilterEnabled());

		LoadGeometry();
		auto& header = *ui.view->header();
		Util::LoadHeaderSectionWidth(header, *this->settings, FIELD_WIDTH_KEY);
		header.setSortIndicator(this->settings->Get(SORT_INDEX_KEY, header.sortIndicatorSection()), this->settings->Get(SORT_ORDER_KEY, header.sortIndicatorOrder()));
	}

	~Impl() override
	{
		auto& header = *ui.view->header();
		Util::SaveHeaderSectionWidth(header, *this->settings, FIELD_WIDTH_KEY);
		settings->Set(SORT_INDEX_KEY,header.sortIndicatorSection());
		settings->Set(SORT_ORDER_KEY,header.sortIndicatorOrder());
		SaveGeometry();
	}

	void Save()
	{
		//		languageFilterController->SetFilteredCodes(ui.checkBoxFilterEnabled->isChecked(), GetSelected(*languageModel->GetModel()));
	}

private:
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

FilterDialog::FilterDialog(const std::shared_ptr<const IParentWidgetProvider>& parentWidgetProvider,
                           std::shared_ptr<ISettings> settings,
                           std::shared_ptr<IFilterController> filterController,
                           std::shared_ptr<ScrollBarController> scrollBarController,
                           QWidget* parent)
	: QDialog(parentWidgetProvider->GetWidget(parent))
	, m_impl(*this, std::move(settings), std::move(filterController), std::move(scrollBarController))
{
	connect(this, &QDialog::accepted, [this] { m_impl->Save(); });
}

FilterDialog::~FilterDialog() = default;
