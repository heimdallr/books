#include "ui_GenreFilterDialog.h"

#include "GenreFilterDialog.h"

#include <set>

#include <QMenu>

#include "interface/logic/IModel.h"

#include "gutil/GeometryRestorable.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

using Role = IGenreModel::Role;

std::set<QString> GetSelected(const QAbstractItemModel& model)
{
	auto list = model.data({}, Role::SelectedList).toStringList();
	std::set<QString> selected;
	std::ranges::move(list, std::inserter(selected, selected.end()));
	return selected;
}

void SetModelData(QAbstractItemModel& model, const int role, const QVariant& value = {}, const QModelIndex& index = {})
{
	model.setData(index, value, role);
}

}

struct GenreFilterDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	QWidget& self;
	PropagateConstPtr<ISettings, std::shared_ptr> settings;
	PropagateConstPtr<IGenreFilterController, std::shared_ptr> genreFilterController;
	PropagateConstPtr<IGenreModel, std::shared_ptr> genreModel;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> scrollBarController;
	std::set<QString> selected;
	std::unordered_set<QString> filtered;
	Ui::GenreFilterDialog ui {};

	Impl(GenreFilterDialog& self,
	     const IGenreFilterProvider& genreFilterProvider,
	     std::shared_ptr<ISettings> settings,
	     std::shared_ptr<IGenreFilterController> genreFilterController,
	     std::shared_ptr<IGenreModel> genreModel,
	     std::shared_ptr<ScrollBarController> scrollBarController)
		: GeometryRestorable(*this, settings, "GenreFilterDialog")
		, GeometryRestorableObserver(self)
		, self { self }
		, settings { std::move(settings) }
		, genreFilterController { std::move(genreFilterController) }
		, genreModel { std::move(genreModel) }
		, scrollBarController { std::move(scrollBarController) }
		, filtered { genreFilterProvider.GetFilteredCodes() }
	{
		ui.setupUi(&self);

		auto* model = this->genreModel->GetModel();

		for (auto [action, role] : std::initializer_list<std::pair<QAction*, int>> {
				 {     ui.actionGenreCheckAll,     Role::CheckAll },
				 {   ui.actionGenreUncheckAll,   Role::UncheckAll },
				 { ui.actionGenreInvertChecks, Role::RevertChecks },
        })
			connect(action, &QAction::triggered, &self, [=] { SetModelData(*model, role); });

		connect(ui.view, &QWidget::customContextMenuRequested, &self, [&] { OnGenresContextMenuRequested(); });
		connect(model,
		        &QAbstractItemModel::modelReset,
		        [this, model]
		        {
					model->setData({}, {}, Role::CheckAll);
					if (!this->filtered.empty())
						model->setData({}, QVariant::fromValue(this->filtered), Role::UncheckAll);
					selected = GetSelected(*model);
				});
		ui.view->setModel(model);
		this->scrollBarController->SetScrollArea(ui.view);
		ui.view->viewport()->installEventFilter(this->scrollBarController.get());

		ui.checkBoxFilterEnabled->setChecked(genreFilterProvider.IsFilterEnabled());

		LoadGeometry();
	}

	~Impl() override
	{
		SaveGeometry();
	}

	void Save()
	{
		const auto current = GetSelected(*genreModel->GetModel());
		for (const auto& item : current)
			filtered.erase(item);

		std::ranges::set_difference(selected, current, std::inserter(filtered, filtered.end()));
		genreFilterController->SetFilteredCodes(ui.checkBoxFilterEnabled->isChecked(), filtered);
	}

private:
	void OnGenresContextMenuRequested() const
	{
		QMenu menu;
		menu.setFont(self.font());
		menu.addAction(ui.actionGenreCheckAll);
		menu.addAction(ui.actionGenreUncheckAll);
		menu.addAction(ui.actionGenreInvertChecks);
		menu.exec(QCursor::pos());
	}

	NON_COPY_MOVABLE(Impl)
};

GenreFilterDialog::GenreFilterDialog(const std::shared_ptr<const IParentWidgetProvider>& parentWidgetProvider,
                                     const std::shared_ptr<const IGenreFilterProvider>& genreFilterProvider,
                                     std::shared_ptr<ISettings> settings,
                                     std::shared_ptr<IGenreFilterController> genreFilterController,
                                     std::shared_ptr<IGenreModel> genreModel,
                                     std::shared_ptr<ScrollBarController> scrollBarController,
                                     QWidget* parent)
	: QDialog(parentWidgetProvider->GetWidget(parent))
	, m_impl(*this, *genreFilterProvider, std::move(settings), std::move(genreFilterController), std::move(genreModel), std::move(scrollBarController))
{
	connect(this, &QDialog::accepted, [this] { m_impl->Save(); });
}

GenreFilterDialog::~GenreFilterDialog() = default;
