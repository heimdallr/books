#include "ui_CollectionCleaner.h"
#include "CollectionCleaner.h"

#include <QMenu>
#include <QPushButton>

#include "fnd/ScopedCall.h"

#include "GuiUtil/GeometryRestorable.h"
#include "GuiUtil/interface/IUiFactory.h"

#include "interface/logic/ICollectionCleaner.h"
#include "interface/logic/IModel.h"
#include "interface/logic/IReaderController.h"

#include "util/localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
using Role = IModel::Role;

constexpr auto CONTEXT = "CollectionCleaner";
constexpr auto BOOKS_NOT_FOUND = QT_TRANSLATE_NOOP("CollectionCleaner", "No books were found in the collection according to the specified criteria");
constexpr auto BOOKS_TO_DELETE = QT_TRANSLATE_NOOP("CollectionCleaner", "There are %1 book(s) found in the collection matching your criteria. Are you sure you want to delete them?");

TR_DEF

void SetModelData(QAbstractItemModel& model, const int role, const QVariant& value = {}, const QModelIndex& index = {})
{
	model.setData(index, value, role);
}

}

class CollectionCleaner::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
	, ICollectionCleaner::IAnalyzeObserver
{
public:
	Impl(CollectionCleaner& self
		, std::shared_ptr<const Util::IUiFactory> uiFactory
		, std::shared_ptr<const IReaderController> readerController
		, std::shared_ptr<const ICollectionCleaner> collectionCleaner
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<IGenreModel> genreModel
		, std::shared_ptr<ILanguageModel> languageModel
	)
		: GeometryRestorable(*this, std::move(settings), CONTEXT)
		, GeometryRestorableObserver(self)
		, m_self{ self }
		, m_uiFactory{ std::move(uiFactory) }
		, m_readerController{ std::move(readerController) }
		, m_collectionCleaner{ std::move(collectionCleaner) }
		, m_genreModel{ std::shared_ptr<IModel>{std::move(genreModel)} }
		, m_languageModel{ std::shared_ptr<IModel>{std::move(languageModel)} }
	{
		m_ui.setupUi(&self);

		m_ui.genres->setModel(m_genreModel->GetModel());
		m_ui.languages->setModel(m_languageModel->GetModel());
		m_ui.languages->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);

		m_ui.progressBar->setVisible(false);

		connect(m_ui.genres, &QWidget::customContextMenuRequested, &m_self, [&] { OnGenresContextMenuRequested(); });
		connect(m_ui.languages, &QWidget::customContextMenuRequested, &m_self, [&] { OnLanguagesContextMenuRequested(); });
		connect(m_ui.languages, &QAbstractItemView::doubleClicked, m_ui.actionLanguageReadRandomBook, &QAction::trigger);

		connect(m_ui.actionLanguageReadRandomBook, &QAction::triggered, &m_self, [&] { OpenRandomBook(); });
		connect(m_ui.actionLanguageCheckAll, &QAction::triggered, &m_self, [&] { SetModelData(*m_ui.languages->model(), Role::CheckAll); });
		connect(m_ui.actionLanguageUncheckAll, &QAction::triggered, &m_self, [&] { SetModelData(*m_ui.languages->model(), Role::UncheckAll); });
		connect(m_ui.actionLanguageInvertChecks, &QAction::triggered, &m_self, [&] { SetModelData(*m_ui.languages->model(), Role::RevertChecks); });
		connect(m_ui.actionGenreCheckAll, &QAction::triggered, &m_self, [&] { SetModelData(*m_ui.genres->model(), Role::CheckAll); });
		connect(m_ui.actionGenreUncheckAll, &QAction::triggered, &m_self, [&] { SetModelData(*m_ui.genres->model(), Role::UncheckAll); });
		connect(m_ui.actionGenreInvertChecks, &QAction::triggered, &m_self, [&] { SetModelData(*m_ui.genres->model(), Role::RevertChecks); });
		connect(m_ui.buttons, &QDialogButtonBox::rejected, &self, [&] { OnCancelClicked(); });
		connect(m_ui.buttons, &QDialogButtonBox::accepted, &self, [&] { Analyze(); });

		Init();
	}

private: // ICollectionCleaner::IAnalyzeCallback
	void AnalyzeFinished(ICollectionCleaner::Books books) override
	{
		if (m_analyzeCanceled)
			return m_self.reject();

		auto dialogGuard = std::make_shared<ScopedCall>([this] { m_self.hide(); }, [this] { m_self.accept(); });

		m_ui.progressBar->setVisible(false);

		if (books.empty())
			return m_uiFactory->ShowInfo(Tr(BOOKS_NOT_FOUND));

		const auto count = books.size();
		if (m_uiFactory->ShowQuestion(Tr(BOOKS_TO_DELETE).arg(count), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return;

		m_collectionCleaner->Remove(std::move(books), [this, dialogGuard = std::move(dialogGuard), count](const bool ok)
		{
			if (ok)
				m_uiFactory->ShowInfo(Loc::Tr(ICollectionCleaner::CONTEXT, ICollectionCleaner::REMOVE_PERMANENTLY_INFO).arg(count));
		});
	}

private:
	void OnGenresContextMenuRequested() const
	{
		QMenu menu;
		menu.setFont(m_self.font());
		menu.addAction(m_ui.actionGenreCheckAll);
		menu.addAction(m_ui.actionGenreUncheckAll);
		menu.addAction(m_ui.actionGenreInvertChecks);
		menu.exec(QCursor::pos());
	}

	void OnLanguagesContextMenuRequested() const
	{
		QMenu menu;
		menu.setFont(m_self.font());
		menu.addAction(m_ui.actionLanguageReadRandomBook);
		menu.addSeparator();
		menu.addAction(m_ui.actionLanguageCheckAll);
		menu.addAction(m_ui.actionLanguageUncheckAll);
		menu.addAction(m_ui.actionLanguageInvertChecks);
		menu.exec(QCursor::pos());
	}

	void OpenRandomBook() const
	{
		const auto lang = m_ui.languages->model()->index(m_ui.languages->currentIndex().row(), 0).data().toString();
		m_readerController->ReadRandomBook(lang);
	}

	void OnCancelClicked()
	{
		m_ui.buttons->button(QDialogButtonBox::StandardButton::Cancel)->setEnabled(false);
		m_analyzeCanceled = true;
		m_ui.progressBar->isVisible()
			? m_collectionCleaner->AnalyzeCancel()
			: m_self.reject();
	}

	void Analyze()
	{
		m_ui.progressBar->setVisible(true);
		m_ui.buttons->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(false);
		m_collectionCleaner->Analyze(*this);
	}

private:
	QDialog& m_self;
	Ui::CollectionCleaner m_ui;
	std::shared_ptr<const Util::IUiFactory> m_uiFactory;
	std::shared_ptr<const IReaderController> m_readerController;
	std::shared_ptr<const ICollectionCleaner> m_collectionCleaner;
	PropagateConstPtr<IModel, std::shared_ptr> m_genreModel;
	PropagateConstPtr<IModel, std::shared_ptr> m_languageModel;
	bool m_analyzeCanceled{ false };
};

CollectionCleaner::CollectionCleaner(std::shared_ptr<const Util::IUiFactory> uiFactory
	, std::shared_ptr<const IReaderController> readerController
	, std::shared_ptr<const ICollectionCleaner> collectionCleaner
	, std::shared_ptr<ISettings> settings
	, std::shared_ptr<IGenreModel> genreModel
	, std::shared_ptr<ILanguageModel> languageModel
	, QWidget *parent
)
	: QDialog(uiFactory->GetParentWidget(parent))
	, m_impl(*this
		, std::move(uiFactory)
		, std::move(readerController)
		, std::move(collectionCleaner)
		, std::move(settings)
		, std::move(genreModel)
		, std::move(languageModel)
	)
{
}

CollectionCleaner::~CollectionCleaner() = default;
