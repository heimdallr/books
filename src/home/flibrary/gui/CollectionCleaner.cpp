#include "ui_CollectionCleaner.h"
#include "CollectionCleaner.h"

#include <QMenu>

#include "GuiUtil/GeometryRestorable.h"
#include "GuiUtil/interface/IUiFactory.h"

#include "interface/logic/IModel.h"
#include "interface/logic/IReaderController.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
using Role = IModel::Role;
}

struct CollectionCleaner::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	Ui::CollectionCleaner ui;
	std::shared_ptr<const IReaderController> readerController;
	PropagateConstPtr<IModel, std::shared_ptr> genreModel;
	PropagateConstPtr<IModel, std::shared_ptr> languageModel;

	Impl(CollectionCleaner& self
		, std::shared_ptr<const IReaderController> readerController
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<IGenreModel> genreModel
		, std::shared_ptr<ILanguageModel> languageModel
	)
		: GeometryRestorable(*this, std::move(settings), "CollectionCleaner")
		, GeometryRestorableObserver(self)
		, readerController{ std::move(readerController) }
		, genreModel{ std::shared_ptr<IModel>{std::move(genreModel)} }
		, languageModel{ std::shared_ptr<IModel>{std::move(languageModel)} }
	{
		ui.setupUi(&self);

		ui.genres->setModel(this->genreModel->GetModel());
		ui.languages->setModel(this->languageModel->GetModel());
		ui.languages->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);

		connect(ui.genres, &QWidget::customContextMenuRequested, &self, [&] { OnGenresContextMenuRequested(self); });
		connect(ui.languages, &QWidget::customContextMenuRequested, &self, [&] { OnLanguagesContextMenuRequested(self); });
		connect(ui.languages, &QAbstractItemView::doubleClicked, ui.actionLanguageReadRandomBook, &QAction::trigger);

		connect(ui.actionLanguageReadRandomBook, &QAction::triggered, &self, [&] { OpenRandomBook(); });
		connect(ui.actionLanguageCheckAll, &QAction::triggered, &self, [&] { SetModelData(*ui.languages->model(), Role::CheckAll); });
		connect(ui.actionLanguageUncheckAll, &QAction::triggered, &self, [&] { SetModelData(*ui.languages->model(), Role::UncheckAll); });
		connect(ui.actionLanguageInvertChecks, &QAction::triggered, &self, [&] { SetModelData(*ui.languages->model(), Role::RevertChecks); });
		connect(ui.actionGenreCheckAll, &QAction::triggered, &self, [&] { SetModelData(*ui.genres->model(), Role::CheckAll); });
		connect(ui.actionGenreUncheckAll, &QAction::triggered, &self, [&] { SetModelData(*ui.genres->model(), Role::UncheckAll); });
		connect(ui.actionGenreInvertChecks, &QAction::triggered, &self, [&] { SetModelData(*ui.genres->model(), Role::RevertChecks); });

		Init();
	}

private:
	void SetModelData(QAbstractItemModel& model, const int role, const QVariant& value = {}, const QModelIndex& index = {})
	{
		model.setData(index, value, role);
	}

	void OnGenresContextMenuRequested(QWidget& self)
	{
		QMenu menu;
		menu.setFont(self.font());
		menu.addAction(ui.actionGenreCheckAll);
		menu.addAction(ui.actionGenreUncheckAll);
		menu.addAction(ui.actionGenreInvertChecks);
		menu.exec(QCursor::pos());
	}

	void OnLanguagesContextMenuRequested(QWidget& self)
	{
		QMenu menu;
		menu.setFont(self.font());
		menu.addAction(ui.actionLanguageReadRandomBook);
		menu.addSeparator();
		menu.addAction(ui.actionLanguageCheckAll);
		menu.addAction(ui.actionLanguageUncheckAll);
		menu.addAction(ui.actionLanguageInvertChecks);
		menu.exec(QCursor::pos());
	}

	void OpenRandomBook() const
	{
		const auto lang = ui.languages->model()->index(ui.languages->currentIndex().row(), 0).data().toString();
		readerController->ReadRandomBook(lang);
	}
};

CollectionCleaner::CollectionCleaner(std::shared_ptr<const Util::IUiFactory> uiFactory
	, std::shared_ptr<const IReaderController> readerController
	, std::shared_ptr<ISettings> settings
	, std::shared_ptr<IGenreModel> genreModel
	, std::shared_ptr<ILanguageModel> languageModel
	, QWidget *parent
)
	: QDialog(uiFactory->GetParentWidget(parent))
	, m_impl(*this
		, std::move(readerController)
		, std::move(settings)
		, std::move(genreModel)
		, std::move(languageModel)
	)
{
}

CollectionCleaner::~CollectionCleaner() = default;
