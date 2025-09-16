#include "ui_LanguageFilterDialog.h"

#include "LanguageFilterDialog.h"

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

std::unordered_set<QString> GetSelected(const QAbstractItemModel& model)
{
	auto list = model.data({}, Role::SelectedList).toStringList();
	std::unordered_set<QString> selected;
	std::ranges::move(list, std::inserter(selected, selected.end()));
	return selected;
}

void SetModelData(QAbstractItemModel& model, const int role, const QVariant& value = {}, const QModelIndex& index = {})
{
	model.setData(index, value, role);
}

}

struct LanguageFilterDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	QWidget& self;
	PropagateConstPtr<ISettings, std::shared_ptr> settings;
	PropagateConstPtr<ILanguageFilterController, std::shared_ptr> languageFilterController;
	PropagateConstPtr<ILanguageModel, std::shared_ptr> languageModel;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> scrollBarController;
	Ui::LanguageFilterDialog ui {};

	Impl(LanguageFilterDialog& self,
	     const ILanguageFilterProvider& languageFilterProvider,
	     std::shared_ptr<ISettings> settings,
	     std::shared_ptr<ILanguageFilterController> languageFilterController,
	     std::shared_ptr<ILanguageModel> languageModel,
	     std::shared_ptr<ScrollBarController> scrollBarController)
		: GeometryRestorable(*this, settings, "LanguageFilterDialog")
		, GeometryRestorableObserver(self)
		, self { self }
		, settings { std::move(settings) }
		, languageFilterController { std::move(languageFilterController) }
		, languageModel { std::move(languageModel) }
		, scrollBarController { std::move(scrollBarController) }
	{
		ui.setupUi(&self);

		auto* model = this->languageModel->GetModel();

		for (auto [action, role] : std::initializer_list<std::pair<QAction*, int>> {
				 {     ui.actionLanguageCheckAll,     Role::CheckAll },
				 {   ui.actionLanguageUncheckAll,   Role::UncheckAll },
				 { ui.actionLanguageInvertChecks, Role::RevertChecks },
        })
			connect(action, &QAction::triggered, &self, [=] { SetModelData(*model, role); });

		connect(ui.view, &QWidget::customContextMenuRequested, &self, [&] { OnLanguagesContextMenuRequested(); });
		connect(model,
		        &QAbstractItemModel::modelReset,
		        [this, model, filtered = languageFilterProvider.GetFilteredCodes()]() mutable
		        {
					if (filtered.empty())
						return;

					QStringList list { filtered.cbegin(), filtered.cend() };
					filtered.clear();
					model->setData({}, QVariant::fromValue(list), Role::SelectedList);
				});
		ui.view->setModel(model);
		this->scrollBarController->SetScrollArea(ui.view);
		ui.view->viewport()->installEventFilter(this->scrollBarController.get());

		ui.checkBoxFilterEnabled->setChecked(languageFilterProvider.IsFilterEnabled());

		LoadGeometry();
		Util::LoadHeaderSectionWidth(*ui.view->horizontalHeader(), *this->settings, FIELD_WIDTH_KEY);
	}

	~Impl() override
	{
		Util::SaveHeaderSectionWidth(*ui.view->horizontalHeader(), *this->settings, FIELD_WIDTH_KEY);
		SaveGeometry();
	}

	void Save()
	{
		languageFilterController->SetFilteredCodes(ui.checkBoxFilterEnabled->isChecked(), GetSelected(*languageModel->GetModel()));
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

LanguageFilterDialog::LanguageFilterDialog(const std::shared_ptr<const IParentWidgetProvider>& parentWidgetProvider,
                                           const std::shared_ptr<const ILanguageFilterProvider>& languageFilterProvider,
                                           std::shared_ptr<ISettings> settings,
                                           std::shared_ptr<ILanguageFilterController> languageFilterController,
                                           std::shared_ptr<ILanguageModel> languageModel,
                                           std::shared_ptr<ScrollBarController> scrollBarController,
                                           QWidget* parent)
	: QDialog(parentWidgetProvider->GetWidget(parent))
	, m_impl(*this, *languageFilterProvider, std::move(settings), std::move(languageFilterController), std::move(languageModel), std::move(scrollBarController))
{
	connect(this, &QDialog::accepted, [this] { m_impl->Save(); });
}

LanguageFilterDialog::~LanguageFilterDialog() = default;
