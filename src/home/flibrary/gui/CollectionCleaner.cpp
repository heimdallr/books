#include "ui_CollectionCleaner.h"

#include "CollectionCleaner.h"

#include <QMenu>
#include <QPushButton>

#include "fnd/FindPair.h"
#include "fnd/ScopedCall.h"

#include "interface/ui/IUiFactory.h"

#include "GuiUtil/GeometryRestorable.h"
#include "util/localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
using Role = IModel::Role;

constexpr auto CONTEXT = ICollectionCleaner::CONTEXT;
constexpr auto BOOKS_NOT_FOUND = QT_TRANSLATE_NOOP("CollectionCleaner", "No books were found in the collection according to the specified criteria");
constexpr auto BOOKS_TO_DELETE = QT_TRANSLATE_NOOP("CollectionCleaner", "There are %1 book(s) found in the collection matching your criteria. Are you sure you want to delete them%2?");
constexpr auto PERMANENTLY = QT_TRANSLATE_NOOP("CollectionCleaner", " permanently, without the possibility of recovery");
constexpr auto ANALYZING = QT_TRANSLATE_NOOP("CollectionCleaner", "Wait. Collection analysis in progress...");
constexpr auto WRONG_SIZES = QT_TRANSLATE_NOOP("CollectionCleaner", "Strange values for minimum and maximum book sizes. Do you want to delete all books?");
constexpr auto LOGICAL_REMOVING_RESULT = QT_TRANSLATE_NOOP("CollectionCleaner", "%1 book(s) deleted");

constexpr auto DELETE_DELETED_KEY = "ui/Cleaner/DeleteDeleted";
constexpr auto DELETE_DUPLICATE_KEY = "ui/Cleaner/DeleteDuplicate";
constexpr auto DELETE_BY_GENRE_KEY = "ui/Cleaner/DeleteByGenre";
constexpr auto DELETE_BY_GENRE_MODE_KEY = "ui/Cleaner/DeleteByGenreMode";
constexpr auto DELETE_BY_LANGUAGE_KEY = "ui/Cleaner/DeleteByLanguage";
constexpr auto GENRE_LIST_KEY = "ui/Cleaner/Genres";
constexpr auto LANGUAGE_LIST_KEY = "ui/Cleaner/Languages";
constexpr auto LANGUAGE_FIELD_WIDTH_KEY = "ui/Cleaner/LanguageFieldWidths";
constexpr auto LANGUAGE_SORT_INDICATOR_COLUMN = "ui/Cleaner/LanguageSortColumn";
constexpr auto LANGUAGE_SORT_INDICATOR_ORDER = "ui/Cleaner/LanguageSortOrder";
constexpr auto MAXIMUM_SIZE = "ui/Cleaner/MaximumSize";
constexpr auto MAXIMUM_SIZE_ENABLED = "ui/Cleaner/MaximumSizeEnabled";
constexpr auto MINIMUM_SIZE = "ui/Cleaner/MinimumSize";
constexpr auto MINIMUM_SIZE_ENABLED = "ui/Cleaner/MinimumSizeEnabled";
constexpr auto MINIMUM_RATE = "ui/Cleaner/MinimumRate";
constexpr auto DELETE_BY_RATE = "ui/Cleaner/DeleteByRate";
constexpr auto DELETE_UNRATED = "ui/Cleaner/DeleteUnrated";

TR_DEF

constexpr std::pair<ICollectionCleaner::CleanGenreMode, const char*> CLEAN_GENRE_MODE[] {
#define ITEM(NAME) { ICollectionCleaner::CleanGenreMode::NAME, #NAME }
	ITEM(Full),
	ITEM(Partial),
#undef ITEM
};

void SetModelData(QAbstractItemModel& model, const int role, const QVariant& value = {}, const QModelIndex& index = {})
{
	model.setData(index, value, role);
}

} // namespace

class CollectionCleaner::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
	, ICollectionCleaner::IAnalyzeObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(CollectionCleaner& self,
	     const ICollectionProvider& collectionProvider,
	     std::shared_ptr<const IUiFactory> uiFactory,
	     std::shared_ptr<const IReaderController> readerController,
	     std::shared_ptr<const ICollectionCleaner> collectionCleaner,
	     std::shared_ptr<const IBookInfoProvider> dataProvider,
	     std::shared_ptr<ISettings> settings,
	     std::shared_ptr<IGenreModel> genreModel,
	     std::shared_ptr<ILanguageModel> languageModel,
	     std::shared_ptr<ScrollBarController> scrollBarControllerGenre,
	     std::shared_ptr<ScrollBarController> scrollBarControllerLanguage)
		: GeometryRestorable(*this, settings, CONTEXT)
		, GeometryRestorableObserver(self)
		, m_self { self }
		, m_uiFactory { std::move(uiFactory) }
		, m_readerController { std::move(readerController) }
		, m_collectionCleaner { std::move(collectionCleaner) }
		, m_dataProvider { std::move(dataProvider) }
		, m_settings { std::move(settings) }
		, m_genreModel { std::shared_ptr<IModel> { std::move(genreModel) } }
		, m_languageModel { std::shared_ptr<IModel> { std::move(languageModel) } }
		, m_scrollBarControllerGenre { std::move(scrollBarControllerGenre) }
		, m_scrollBarControllerLanguage { std::move(scrollBarControllerLanguage) }
		, m_additionalWidgetCallback { m_uiFactory->GetAdditionalWidgetCallback() }
	{
		m_ui.setupUi(&self);

		m_ui.genres->setModel(m_genreModel->GetModel());
		m_ui.genres->viewport()->installEventFilter(m_scrollBarControllerGenre.get());
		m_scrollBarControllerGenre->SetScrollArea(m_ui.genres);

		m_ui.languages->setModel(m_languageModel->GetModel());
		m_ui.languages->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);
		m_ui.languages->viewport()->installEventFilter(m_scrollBarControllerLanguage.get());
		m_scrollBarControllerLanguage->SetScrollArea(m_ui.languages);

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
		connect(m_ui.btnCancel, &QAbstractButton::clicked, &self, [&] { OnCancelClicked(); });
		connect(m_ui.btnAnalyze, &QAbstractButton::clicked, &self, [&] { Analyze(); });

		connect(m_ui.minimumSize, &QSpinBox::valueChanged, &m_self, [this](const int value) { m_ui.minimumSize->setSingleStep(std::max(1, value / 2)); });
		connect(m_ui.maximumSize, &QSpinBox::valueChanged, &m_self, [this](const int value) { m_ui.maximumSize->setSingleStep(std::max(1, value / 2)); });

		m_ui.removeForever->setEnabled(collectionProvider.ActiveCollectionExists() && collectionProvider.GetActiveCollection().destructiveOperationsAllowed);

		Load();

		auto label = new QLabel(Tr(ANALYZING));
		label->setAlignment(Qt::AlignCenter);

		auto layout = new QVBoxLayout(m_ui.progressBar);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->addWidget(label);

		LoadGeometry();
	}

	~Impl() override
	{
		SaveGeometry();
		Save();
	}

private: // ICollectionCleaner::IAnalyzeObserver
	void AnalyzeFinished(ICollectionCleaner::Books books) override
	{
		if (m_analyzeCanceled)
			return m_additionalWidgetCallback(ICollectionCleaner::State::Canceled);

		OnAnalyzing(false);

		if (books.empty())
			return m_uiFactory->ShowInfo(Tr(BOOKS_NOT_FOUND));

		const auto count = books.size();
		if (m_uiFactory->ShowQuestion(Tr(BOOKS_TO_DELETE).arg(count).arg(m_ui.removeForever->isChecked() ? Tr(PERMANENTLY) : ""), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return;

		auto dialogGuard =
			std::make_shared<ScopedCall>([this] { m_additionalWidgetCallback(ICollectionCleaner::State::Started); }, [this] { m_additionalWidgetCallback(ICollectionCleaner::State::Finished); });

		QEventLoop eventLoop;

		if (m_ui.removeForever->isChecked())
		{
			m_collectionCleaner->RemovePermanently(std::move(books),
			                                       [this, dialogGuard = std::move(dialogGuard), count, &eventLoop](const bool ok)
			                                       {
													   if (ok)
														   m_uiFactory->ShowInfo(Tr(ICollectionCleaner::REMOVE_PERMANENTLY_INFO).arg(count));

													   eventLoop.exit();
												   });
		}
		else
		{
			m_collectionCleaner->Remove(std::move(books),
			                            [this, dialogGuard = std::move(dialogGuard), count, &eventLoop](const bool ok)
			                            {
											if (ok)
												m_uiFactory->ShowInfo(Tr(LOGICAL_REMOVING_RESULT).arg(count));

											eventLoop.exit();
											if (ok)
												m_dataProvider->RequestBooks(true);
										});
		}

		eventLoop.exec();
	}

	bool IsPermanently() const override
	{
		return m_ui.removeForever->isChecked();
	}

	bool NeedDeleteMarkedAsDeleted() const override
	{
		return m_ui.removeRemoved->isEnabled() && m_ui.removeRemoved->isChecked();
	}

	bool NeedDeleteDuplicates() const override
	{
		return m_ui.duplicates->isChecked();
	}

	bool NeedDeleteUnrated() const override
	{
		return m_ui.unrated->isChecked();
	}

	QStringList GetLanguages() const override
	{
		return m_ui.groupBoxLanguages->isChecked() ? m_ui.languages->model()->data({}, Role::SelectedList).toStringList() : QStringList {};
	}

	QStringList GetGenres() const override
	{
		return m_ui.groupBoxGenres->isChecked() ? m_ui.genres->model()->data({}, Role::SelectedList).toStringList() : QStringList {};
	}

	ICollectionCleaner::CleanGenreMode GetCleanGenreMode() const override
	{
		return m_ui.genresMatchFull->isChecked()    ? ICollectionCleaner::CleanGenreMode::Full
		     : m_ui.genresMatchPartial->isChecked() ? ICollectionCleaner::CleanGenreMode::Partial
		                                            : ICollectionCleaner::CleanGenreMode::None;
	}

	std::optional<size_t> GetMinimumBookSize() const override
	{
		return m_ui.minimumSizeEnabled->isChecked() ? std::optional { m_ui.minimumSize->value() } : std::nullopt;
	}

	std::optional<size_t> GetMaximumBookSize() const override
	{
		return m_ui.maximumSizeEnabled->isChecked() ? std::optional { m_ui.maximumSize->value() } : std::nullopt;
	}

	std::optional<double> GetMaximumLibRate() const override
	{
		return m_ui.ratedLess->isChecked() ? std::optional { m_ui.minimumRate->value() } : std::nullopt;
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
		m_ui.btnCancel->setEnabled(false);
		m_analyzeCanceled = true;
		m_ui.progressBar->isVisible() ? m_collectionCleaner->AnalyzeCancel() : m_additionalWidgetCallback(QDialog::Rejected);
	}

	void Analyze()
	{
		if (m_ui.maximumSizeEnabled->isChecked() && m_ui.minimumSizeEnabled->isChecked() && m_ui.maximumSize->value() <= m_ui.minimumSize->value()
		    && m_uiFactory->ShowQuestion(WRONG_SIZES, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;

		OnAnalyzing(true);
		m_collectionCleaner->Analyze(*this);
	}

	void OnAnalyzing(const bool running) const
	{
		m_ui.progressBar->setVisible(running);
		m_ui.btnAnalyze->setEnabled(!running);
	}

	void Load()
	{
		switch (FindFirst(CLEAN_GENRE_MODE,
		                  m_settings->Get(DELETE_BY_GENRE_MODE_KEY, FindSecond(CLEAN_GENRE_MODE, GetCleanGenreMode())).toStdString().data(),
		                  ICollectionCleaner::CleanGenreMode::None,
		                  PszComparer {}))
		{
			case ICollectionCleaner::CleanGenreMode::Full:
				m_ui.genresMatchFull->setChecked(true);
				break;
			case ICollectionCleaner::CleanGenreMode::Partial:
				m_ui.genresMatchPartial->setChecked(true);
				break;
			case ICollectionCleaner::CleanGenreMode::None:
				break;
		}

		auto* header = m_ui.languages->horizontalHeader();
		header->setSortIndicator(m_settings->Get(LANGUAGE_SORT_INDICATOR_COLUMN, header->sortIndicatorSection()), m_settings->Get(LANGUAGE_SORT_INDICATOR_ORDER, header->sortIndicatorOrder()));

		m_ui.removeRemoved->setChecked(m_settings->Get(DELETE_DELETED_KEY, m_ui.removeRemoved->isChecked()));
		m_ui.duplicates->setChecked(m_settings->Get(DELETE_DUPLICATE_KEY, m_ui.duplicates->isChecked()));
		m_ui.groupBoxGenres->setChecked(m_settings->Get(DELETE_BY_GENRE_KEY, m_ui.groupBoxGenres->isChecked()));
		m_ui.groupBoxLanguages->setChecked(m_settings->Get(DELETE_BY_LANGUAGE_KEY, m_ui.groupBoxLanguages->isChecked()));
		m_ui.genres->model()->setData({}, m_settings->Get(GENRE_LIST_KEY, m_ui.genres->model()->data({}, Role::SelectedList)), Role::SelectedList);
		m_ui.languages->model()->setData({}, m_settings->Get(LANGUAGE_LIST_KEY, m_ui.languages->model()->data({}, Role::SelectedList)), Role::SelectedList);
		m_ui.maximumSize->setValue(m_settings->Get(MAXIMUM_SIZE, m_ui.maximumSize->value()));
		m_ui.minimumSize->setValue(m_settings->Get(MINIMUM_SIZE, m_ui.minimumSize->value()));
		m_ui.maximumSizeEnabled->setChecked(m_settings->Get(MAXIMUM_SIZE_ENABLED, m_ui.maximumSizeEnabled->isChecked()));
		m_ui.minimumSizeEnabled->setChecked(m_settings->Get(MINIMUM_SIZE_ENABLED, m_ui.minimumSizeEnabled->isChecked()));
		m_ui.ratedLess->setChecked(m_settings->Get(DELETE_BY_RATE, m_ui.ratedLess->isChecked()));
		m_ui.unrated->setChecked(m_settings->Get(DELETE_UNRATED, m_ui.unrated->isChecked()));
		m_ui.minimumRate->setValue(m_settings->Get(MINIMUM_RATE, m_ui.minimumRate->value()));

		if (const auto var = m_settings->Get(LANGUAGE_FIELD_WIDTH_KEY, QVariant {}); var.isValid())
		{
			const auto widths = var.value<QVector<int>>();
			for (auto i = 0, sz = std::min(header->count() - 1, static_cast<int>(widths.size())); i < sz; ++i)
				header->resizeSection(i, widths[i]);
		}
	}

	void Save()
	{
		const auto* header = m_ui.languages->horizontalHeader();

		m_settings->Set(DELETE_BY_GENRE_MODE_KEY, FindSecond(CLEAN_GENRE_MODE, GetCleanGenreMode()));
		m_settings->Set(DELETE_DELETED_KEY, m_ui.removeRemoved->isChecked());
		m_settings->Set(DELETE_DUPLICATE_KEY, m_ui.duplicates->isChecked());
		m_settings->Set(DELETE_BY_GENRE_KEY, m_ui.groupBoxGenres->isChecked());
		m_settings->Set(DELETE_BY_LANGUAGE_KEY, m_ui.groupBoxLanguages->isChecked());
		m_settings->Set(GENRE_LIST_KEY, m_ui.genres->model()->data({}, Role::SelectedList));
		m_settings->Set(LANGUAGE_LIST_KEY, m_ui.languages->model()->data({}, Role::SelectedList));
		m_settings->Set(LANGUAGE_SORT_INDICATOR_COLUMN, header->sortIndicatorSection());
		m_settings->Set(LANGUAGE_SORT_INDICATOR_ORDER, header->sortIndicatorOrder());
		m_settings->Set(MAXIMUM_SIZE, m_ui.maximumSize->value());
		m_settings->Set(MINIMUM_SIZE, m_ui.minimumSize->value());
		m_settings->Set(MAXIMUM_SIZE_ENABLED, m_ui.maximumSizeEnabled->isChecked());
		m_settings->Set(MINIMUM_SIZE_ENABLED, m_ui.minimumSizeEnabled->isChecked());
		m_settings->Set(DELETE_BY_RATE, m_ui.ratedLess->isChecked());
		m_settings->Set(DELETE_UNRATED, m_ui.unrated->isChecked());
		m_settings->Set(MINIMUM_RATE, m_ui.minimumRate->value());

		QVector<int> widths;
		for (auto i = 0, sz = header->count() - 1; i < sz; ++i)
			widths << header->sectionSize(i);
		m_settings->Set(LANGUAGE_FIELD_WIDTH_KEY, QVariant::fromValue(widths));
	}

private:
	QWidget& m_self;
	Ui::CollectionCleaner m_ui;
	std::shared_ptr<const IUiFactory> m_uiFactory;
	std::shared_ptr<const IReaderController> m_readerController;
	std::shared_ptr<const ICollectionCleaner> m_collectionCleaner;
	std::shared_ptr<const IBookInfoProvider> m_dataProvider;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<IModel, std::shared_ptr> m_genreModel;
	PropagateConstPtr<IModel, std::shared_ptr> m_languageModel;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> m_scrollBarControllerGenre;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> m_scrollBarControllerLanguage;
	IUiFactory::AdditionalWidgetCallback m_additionalWidgetCallback;
	bool m_analyzeCanceled { false };
};

CollectionCleaner::CollectionCleaner(const std::shared_ptr<const ICollectionProvider>& collectionProvider,
                                     std::shared_ptr<const IUiFactory> uiFactory,
                                     std::shared_ptr<const IReaderController> readerController,
                                     std::shared_ptr<const ICollectionCleaner> collectionCleaner,
                                     std::shared_ptr<const IBookInfoProvider> dataProvider,
                                     std::shared_ptr<ISettings> settings,
                                     std::shared_ptr<IGenreModel> genreModel,
                                     std::shared_ptr<ILanguageModel> languageModel,
                                     std::shared_ptr<ScrollBarController> scrollBarControllerGenre,
                                     std::shared_ptr<ScrollBarController> scrollBarControllerLanguage,
                                     QWidget* parent)
	: QWidget(uiFactory->GetParentWidget(parent))
	, m_impl(*this,
             *collectionProvider,
             std::move(uiFactory),
             std::move(readerController),
             std::move(collectionCleaner),
             std::move(dataProvider),
             std::move(settings),
             std::move(genreModel),
             std::move(languageModel),
             std::move(scrollBarControllerGenre),
             std::move(scrollBarControllerLanguage))
{
}

CollectionCleaner::~CollectionCleaner() = default;
