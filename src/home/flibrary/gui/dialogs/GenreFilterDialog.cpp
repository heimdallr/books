#include "ui_GenreFilterDialog.h"

#include "GenreFilterDialog.h"

#include "interface/logic/IModel.h"

#include "GuiUtil/GeometryRestorable.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

std::set<QString> GetSelected(const QAbstractItemModel& model)
{
	auto list = model.data({}, IGenreModel::Role::SelectedList).toStringList();
	std::set<QString> selected;
	std::ranges::move(list, std::inserter(selected, selected.end()));
	return selected;
}

}

struct GenreFilterDialog::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	PropagateConstPtr<IGenreFilterController, std::shared_ptr> genreFilterController;
	PropagateConstPtr<IGenreModel, std::shared_ptr> genreModel;
	std::set<QString> selected;
	std::unordered_set<QString> filtered { genreFilterController->GetFilteredGenreCodes() };
	Ui::GenreFilterDialog ui {};

	Impl(GenreFilterDialog& self,
	     std::unordered_set<QString> visibleGenres,
	     std::shared_ptr<ISettings> settings,
	     std::shared_ptr<IGenreFilterController> genreFilterController,
	     std::shared_ptr<IGenreModel> genreModel)
		: GeometryRestorable(*this, std::move(settings), "GenreFilterDialog")
		, GeometryRestorableObserver(self)
		, genreFilterController { std::move(genreFilterController) }
		, genreModel { std::move(genreModel) }
	{
		ui.setupUi(&self);

		auto* model = this->genreModel->GetModel();

		connect(model,
		        &QAbstractItemModel::modelReset,
		        [this, model]
		        {
					model->setData({}, {}, IGenreModel::Role::CheckAll);
					if (!filtered.empty())
						model->setData({}, QVariant::fromValue(filtered), IGenreModel::Role::UncheckAll);
					selected = GetSelected(*model);
				});
		model->setData({}, QVariant::fromValue(std::move(visibleGenres)), IGenreModel::Role::VisibleGenreCodes);
		ui.view->setModel(model);

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
		genreFilterController->SetFilteredGenres(filtered);
	}

	NON_COPY_MOVABLE(Impl)
};

GenreFilterDialog::GenreFilterDialog(const std::shared_ptr<const IUiFactory>& uiFactory,
                                     std::shared_ptr<ISettings> settings,
                                     std::shared_ptr<IGenreFilterController> genreFilterController,
                                     std::shared_ptr<IGenreModel> genreModel,
                                     QWidget* parent)
	: QDialog(uiFactory->GetParentWidget(parent))
	, m_impl(*this, uiFactory->GetVisibleGenres(), std::move(settings), std::move(genreFilterController), std::move(genreModel))
{
	connect(this, &QDialog::accepted, [this] { m_impl->Save(); });
}

GenreFilterDialog::~GenreFilterDialog() = default;
