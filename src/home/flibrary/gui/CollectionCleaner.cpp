#include "ui_CollectionCleaner.h"
#include "CollectionCleaner.h"

#include "GuiUtil/GeometryRestorable.h"
#include "GuiUtil/interface/IUiFactory.h"

#include "interface/logic/ILanguageModel.h"
#include "interface/logic/IReaderController.h"

using namespace HomeCompa;
using namespace Flibrary;

struct CollectionCleaner::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	Ui::CollectionCleaner ui;
	std::shared_ptr<const IReaderController> readerController;
	PropagateConstPtr<ILanguageModel, std::shared_ptr> languageModel;

	Impl(CollectionCleaner& self
		, std::shared_ptr<const IReaderController> readerController
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<ILanguageModel> languageModel
	)
		: GeometryRestorable(*this, std::move(settings), "CollectionCleaner")
		, GeometryRestorableObserver(self)
		, readerController{ std::move(readerController) }
		, languageModel{ std::move(languageModel) }
	{
		ui.setupUi(&self);
		ui.languages->setModel(this->languageModel->GetModel());
		ui.languages->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);

		connect(ui.languages, &QAbstractItemView::doubleClicked, &self, [&] { OpenRandomBook(); });

		Init();
	}

private:
	void OpenRandomBook() const
	{
		const auto lang = ui.languages->model()->index(ui.languages->currentIndex().row(), 0).data().toString();
		readerController->ReadRandomBook(lang);
	}
};

CollectionCleaner::CollectionCleaner(std::shared_ptr<const Util::IUiFactory> uiFactory
	, std::shared_ptr<const IReaderController> readerController
	, std::shared_ptr<ISettings> settings
	, std::shared_ptr<ILanguageModel> languageModel
	, QWidget *parent
)
	: QDialog(uiFactory->GetParentWidget(parent))
	, m_impl(*this
		, std::move(readerController)
		, std::move(settings)
		, std::move(languageModel)
	)
{
}

CollectionCleaner::~CollectionCleaner() = default;
