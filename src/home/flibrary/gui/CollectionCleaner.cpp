#include "ui_CollectionCleaner.h"
#include "CollectionCleaner.h"

#include "GuiUtil/GeometryRestorable.h"

#include "interface/logic/ILanguageModel.h"

using namespace HomeCompa::Flibrary;

struct CollectionCleaner::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	Ui::CollectionCleaner ui;
	PropagateConstPtr<ILanguageModel, std::shared_ptr> languageModel;

	Impl(CollectionCleaner& self
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<ILanguageModel> languageModel
	)
		: GeometryRestorable(*this, std::move(settings), "CollectionCleaner")
		, GeometryRestorableObserver(self)
		, languageModel{ std::move(languageModel) }
	{
		ui.setupUi(&self);
		ui.languages->setModel(this->languageModel->GetModel());
		ui.languages->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);
		Init();
	}
};

CollectionCleaner::CollectionCleaner(std::shared_ptr<ISettings> settings
	, std::shared_ptr<ILanguageModel> languageModel
	, QWidget *parent
)
	: QDialog(parent)
	, m_impl(*this
		, std::move(settings)
		, std::move(languageModel)
	)
{
}

CollectionCleaner::~CollectionCleaner() = default;
