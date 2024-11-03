#include "ui_MainWindow.h"
#include "MainWindow.h"

#include <QActionGroup>
#include <QPainter>
#include <QTimer>

#include <plog/Log.h>

#include "AnnotationWidget.h"
#include "config/version.h"
#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"
#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/ICollectionUpdateChecker.h"
#include "interface/logic/ICommandLine.h"
#include "interface/logic/ILogController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IScriptController.h"
#include "interface/logic/ITreeViewController.h"
#include "interface/logic/IUpdateChecker.h"
#include "interface/logic/IUserDataController.h"
#include "interface/ui/dialogs/IScriptDialog.h"
#include "interface/ui/ILineOption.h"
#include "interface/ui/IUiFactory.h"
#include "LocaleController.h"
#include "logging/LogAppender.h"
#include "LogItemDelegate.h"
#include "ProgressBar.h"
#include "TreeView.h"
#include "interface/logic/IDatabaseChecker.h"
#include "GuiUtil/GeometryRestorable.h"
#include "GuiUtil/interface/IParentWidgetProvider.h"
#include "util/FunctorExecutionForwarder.h"
#include "util/ISettings.h"
#include "util/serializer/Font.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto MAIN_WINDOW = "MainWindow";
constexpr auto CONTEXT = "MainWindow";
constexpr auto FONT_DIALOG_TITLE = QT_TRANSLATE_NOOP("MainWindow", "Select font");
constexpr auto CONFIRM_RESTORE_DEFAULT_SETTINGS = QT_TRANSLATE_NOOP("MainWindow", "Are you sure you want to return to default settings?");
constexpr auto DATABASE_BROKEN = QT_TRANSLATE_NOOP("MainWindow", "Database file \"%1\" is probably corrupted");

constexpr auto LOG_SEVERITY_KEY = "ui/LogSeverity";
constexpr auto SHOW_ANNOTATION_KEY = "ui/View/Annotation";
constexpr auto SHOW_ANNOTATION_CONTENT_KEY = "ui/View/AnnotationContent";
constexpr auto SHOW_ANNOTATION_COVER_KEY = "ui/View/AnnotationCover";
constexpr auto SHOW_REMOVED_BOOKS_KEY = "ui/View/RemovedBooks";
constexpr auto SHOW_STATUS_BAR_KEY = "ui/View/Status";
TR_DEF

}

class MainWindow::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
	, ICollectionController::IObserver
	, ILineOption::IObserver
	, virtual plog::IAppender
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(MainWindow & self
		, const std::shared_ptr<const ILogicFactory>& logicFactory
		, std::shared_ptr<const IUiFactory> uiFactory
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<ICollectionController> collectionController
		, std::shared_ptr<const ICollectionUpdateChecker> collectionUpdateChecker
		, std::shared_ptr<IParentWidgetProvider> parentWidgetProvider
		, std::shared_ptr<AnnotationWidget> annotationWidget
		, std::shared_ptr<LocaleController> localeController
		, std::shared_ptr<ILogController> logController
		, std::shared_ptr<QWidget> progressBar
		, std::shared_ptr<QStyledItemDelegate> logItemDelegate
		, std::shared_ptr<ICommandLine> commandLine
		, std::shared_ptr<ILineOption> lineOption
		, std::shared_ptr<IDatabaseChecker> databaseChecker
	)
		: GeometryRestorable(*this, settings, MAIN_WINDOW)
		, GeometryRestorableObserver(self)
		, m_self(self)
		, m_logicFactory(logicFactory)
		, m_uiFactory(std::move(uiFactory))
		, m_settings(std::move(settings))
		, m_collectionController(std::move(collectionController))
		, m_parentWidgetProvider(std::move(parentWidgetProvider))
		, m_annotationWidget(std::move(annotationWidget))
		, m_localeController(std::move(localeController))
		, m_logController(std::move(logController))
		, m_progressBar(std::move(progressBar))
		, m_logItemDelegate(std::move(logItemDelegate))
		, m_lineOption(std::move(lineOption))
		, m_booksWidget(m_uiFactory->CreateTreeViewWidget(ItemType::Books))
		, m_navigationWidget(m_uiFactory->CreateTreeViewWidget(ItemType::Navigation))
		, m_themeActionGroup(new QActionGroup(&m_self))
	{
		Setup();
		ConnectActions();
		CreateLogMenu();
		CreateCollectionsMenu();
		Init();
		QTimer::singleShot(0, [&, commandLine = std::move(commandLine), collectionUpdateChecker = std::move(collectionUpdateChecker), databaseChecker = std::move(databaseChecker)]() mutable
		{
			if (m_collectionController->IsEmpty() || !commandLine->GetInpx().empty())
			{
				if (!m_ui.actionShowLog->isChecked())
					m_ui.actionShowLog->trigger();
				return m_collectionController->AddCollection(commandLine->GetInpx());
			}

			if (!databaseChecker->IsDatabaseValid())
			{
				m_uiFactory->ShowWarning(Tr(DATABASE_BROKEN).arg(m_collectionController->GetActiveCollection()->database));
				return QCoreApplication::exit(Constant::APP_FAILED);
			}

			auto & collectionUpdateCheckerRef = *collectionUpdateChecker;
			collectionUpdateCheckerRef.CheckForUpdate([collectionUpdateChecker = std::move(collectionUpdateChecker)] (bool) mutable { collectionUpdateChecker.reset(); });
		});

		CheckForUpdates(false);
	}

	~Impl() override
	{
		m_collectionController->UnregisterObserver(this);
	}

private: // ICollectionController::IObserver
	void OnActiveCollectionChanged() override
	{
		Reboot();
	}

	void OnNewCollectionCreating(const bool running) override
	{
		if (m_ui.actionShowLog->isChecked() != running)
			m_ui.actionShowLog->trigger();
	}

private: // plog::IAppender
	void write(const plog::Record & record) override
	{
		if (!m_ui.statusBar->isVisible())
			return;

		QString message = record.getMessage();
		m_forwarder.Forward([&, message = std::move(message)]
		{
			m_ui.statusBar->showMessage(message, 2000);
		});
	}

private: // ILineOption::IObserver
	void OnOptionEditing(const QString & value) override
	{
		const auto books = ILogicFactory::GetExtractedBooks(m_booksWidget->GetView()->model(), m_booksWidget->GetView()->currentIndex());
		if (books.empty())
			return;

		auto scriptTemplate = value;
		ILogicFactory::FillScriptTemplate(scriptTemplate, books.front());
		PLOGI << scriptTemplate;
	}

	void OnOptionEditingFinished(const QString & /*value*/) override
	{
		m_ui.settingsLineEdit->actions().clear();
		QTimer::singleShot(0, [&] { m_lineOption->Unregister(this); });
	}

private:
	void Setup()
	{
		m_ui.setupUi(&m_self);

		m_self.setWindowTitle(PRODUCT_ID);

		m_parentWidgetProvider->SetWidget(&m_self);

		m_ui.navigationWidget->layout()->addWidget(m_navigationWidget.get());
		m_ui.annotationWidget->layout()->addWidget(m_annotationWidget.get());
		m_ui.booksWidget->layout()->addWidget(m_booksWidget.get());
		m_ui.booksWidget->layout()->addWidget(m_progressBar.get());

		m_localeController->Setup(*m_ui.menuLanguage);

		m_ui.logView->setModel(m_logController->GetModel());
		m_ui.logView->setItemDelegate(m_logItemDelegate.get());

		m_ui.settingsLineEdit->setVisible(false);
		m_lineOption->SetLineEdit(m_ui.settingsLineEdit);

		m_themeActionGroup->setExclusive(true);

		OnObjectVisibleChanged(m_booksWidget.get(), &TreeView::ShowRemoved, m_ui.actionShowRemoved, m_ui.actionHideRemoved, m_settings->Get(SHOW_REMOVED_BOOKS_KEY, true));
		OnObjectVisibleChanged(m_ui.annotationWidget, &QWidget::setVisible, m_ui.actionShowAnnotation, m_ui.menuAnnotation->menuAction(), m_settings->Get(SHOW_ANNOTATION_KEY, true));
		OnObjectVisibleChanged(m_annotationWidget.get(), &AnnotationWidget::ShowContent, m_ui.actionShowAnnotationContent, m_ui.actionHideAnnotationContent, m_settings->Get(SHOW_ANNOTATION_CONTENT_KEY, true));
		OnObjectVisibleChanged(m_annotationWidget.get(), &AnnotationWidget::ShowCover, m_ui.actionShowAnnotationCover, m_ui.actionHideAnnotationCover, m_settings->Get(SHOW_ANNOTATION_COVER_KEY, true));
		OnObjectVisibleChanged<QStatusBar>(m_ui.statusBar, &QWidget::setVisible, m_ui.actionShowStatusBar, m_ui.actionHideStatusBar, m_settings->Get(SHOW_STATUS_BAR_KEY, true));
		m_ui.actionPermanentLanguageFilter->setChecked(m_settings->Get(Constant::Settings::KEEP_RECENT_LANG_FILTER_KEY, false));

		if (const auto severity = m_settings->Get(LOG_SEVERITY_KEY); severity.isValid())
			m_logController->SetSeverity(severity.toInt());

		if (const auto activeCollection = m_collectionController->GetActiveCollection())
			m_self.setWindowTitle(QString("%1 - %2").arg(PRODUCT_ID).arg(activeCollection->name));
	}

	void ConnectActions()
	{
		const auto incrementFontSize = [&] (const int value)
		{
			const auto fontSize = m_settings->Get(Constant::Settings::FONT_SIZE_KEY, Constant::Settings::FONT_SIZE_DEFAULT);
			m_settings->Set(Constant::Settings::FONT_SIZE_KEY, fontSize + value);
		};
		connect(m_ui.actionFontSizeUp, &QAction::triggered, &m_self, [=]
		{
			incrementFontSize(1);
		});
		connect(m_ui.actionFontSizeDown, &QAction::triggered, &m_self, [=]
		{
			incrementFontSize(-1);
		});

		connect(m_ui.actionExit, &QAction::triggered, &m_self, []
		{
			QCoreApplication::exit();
		});
		connect(m_ui.actionAddNewCollection, &QAction::triggered, &m_self, [&]
		{
			m_collectionController->AddCollection({});
		});
		connect(m_ui.actionCheckForUpdates, &QAction::triggered, &m_self, [&]
		{
			CheckForUpdates(true);
		});
		connect(m_ui.actionAbout, &QAction::triggered, &m_self, [&]
		{
			m_uiFactory->ShowAbout();
		});
		connect(m_localeController.get(), &LocaleController::LocaleChanged, &m_self, [&]
		{
			Reboot();
		});
		connect(m_ui.actionRemoveCollection, &QAction::triggered, &m_self, [&]
		{
			m_collectionController->RemoveCollection();
		});
		connect(m_ui.logView, &QWidget::customContextMenuRequested, &m_self, [&] (const QPoint & pos)
		{
			m_ui.menuLog->exec(m_ui.logView->viewport()->mapToGlobal(pos));
		});
		connect(m_ui.actionShowLog, &QAction::triggered, &m_self, [&] (const bool checked)
		{
			m_ui.stackedWidget->setCurrentIndex(checked ? 1 : 0);
		});
		connect(m_ui.actionClearLog, &QAction::triggered, &m_self, [&]
		{
			m_logController->Clear();
		});
		connect(m_ui.actionShowCollectionStatistics, &QAction::triggered, &m_self, [&]
		{
			m_logController->ShowCollectionStatistics();
			if (!m_ui.actionShowLog->isChecked())
				m_ui.actionShowLog->trigger();
		});
		connect(m_ui.actionFontSettings, &QAction::triggered, &m_self, [&]
		{
			if (const auto font = m_uiFactory->GetFont(Tr(FONT_DIALOG_TITLE), m_self.font()))
			{
				const SettingsGroup group(*m_settings, Constant::Settings::FONT_KEY);
				Util::Serialize(*font, *m_settings);
			}
		});
		connect(m_ui.actionRestoreDefaultSettings, &QAction::triggered, &m_self, [&]
		{
			if (m_uiFactory->ShowQuestion(Tr(CONFIRM_RESTORE_DEFAULT_SETTINGS), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
				return;

			m_settings->Remove("ui");
			Reboot();
		});
		connect(m_ui.actionExportUserData, &QAction::triggered, &m_self, [&]
		{
			auto controller = ILogicFactory::Lock(m_logicFactory)->CreateUserDataController();
			controller->Backup([controller] () mutable
			{
				controller.reset();
			});
		});
		connect(m_ui.actionImportUserData, &QAction::triggered, &m_self, [&]
		{
			auto controller = ILogicFactory::Lock(m_logicFactory)->CreateUserDataController();
			controller->Restore([controller] () mutable
			{
				controller.reset();
			});
		});

		connect(m_ui.actionTestLogColors, &QAction::triggered, &m_self, [&]
		{
			if (!m_ui.actionShowLog->isChecked())
				m_ui.actionShowLog->trigger();

			m_logController->TestColors();
		});

		connect(m_navigationWidget.get(), &TreeView::NavigationModeNameChanged, m_booksWidget.get(), &TreeView::SetNavigationModeName);
		connect(m_ui.actionHideAnnotation, &QAction::visibleChanged, &m_self, [&]
		{
			m_ui.menuAnnotation->menuAction()->setVisible(m_ui.actionHideAnnotation->isVisible());
		});

		connect(m_ui.actionScripts, &QAction::triggered, &m_self, [&]
		{
			m_uiFactory->CreateScriptDialog()->Exec();
		});

		connect(m_ui.actionExportTempate, &QAction::triggered, &m_self, [&]
		{
			IScriptController::SetMacroActions(m_ui.settingsLineEdit);
			m_lineOption->Register(this);
			m_lineOption->SetSettingsKey(Constant::Settings::EXPORT_TEMPLATE_KEY, IScriptController::GetDefaultOutputFileNameTemplate());
		});

		connect(m_ui.menuBook, &QMenu::aboutToShow, &m_self, [&]
		{
			m_ui.menuBook->clear();
			m_booksWidget->FillMenu(*m_ui.menuBook);
		});

		connect(m_ui.actionPermanentLanguageFilter, &QAction::triggered, &m_self, [&](const bool checked)
		{
			m_settings->Set(Constant::Settings::KEEP_RECENT_LANG_FILTER_KEY, checked);
		});

		ConnectShowHide(m_booksWidget.get(), &TreeView::ShowRemoved, m_ui.actionShowRemoved, m_ui.actionHideRemoved, SHOW_REMOVED_BOOKS_KEY);
		ConnectShowHide(m_ui.annotationWidget, &QWidget::setVisible, m_ui.actionShowAnnotation, m_ui.actionHideAnnotation, SHOW_ANNOTATION_KEY);
		ConnectShowHide(m_annotationWidget.get(), &AnnotationWidget::ShowContent, m_ui.actionShowAnnotationContent, m_ui.actionHideAnnotationContent, SHOW_ANNOTATION_CONTENT_KEY);
		ConnectShowHide(m_annotationWidget.get(), &AnnotationWidget::ShowCover, m_ui.actionShowAnnotationCover, m_ui.actionHideAnnotationCover, SHOW_ANNOTATION_COVER_KEY);
		ConnectShowHide<QStatusBar>(m_ui.statusBar, &QWidget::setVisible, m_ui.actionShowStatusBar, m_ui.actionHideStatusBar, SHOW_STATUS_BAR_KEY);
	}

	void CreateLogMenu()
	{
		auto * group = new QActionGroup(&m_self);
		const auto currentSeverity = m_logController->GetSeverity();
		std::ranges::for_each(m_logController->GetSeverities(), [&, n = 0] (const char * name)mutable
		{
			auto * action = m_ui.menuLogVerbosityLevel->addAction(Loc::Tr(Loc::Ctx::LOGGING, name), [&, n]
			{
				m_settings->Set(LOG_SEVERITY_KEY, n);
				m_logController->SetSeverity(n);
			});
			action->setCheckable(true);
			action->setChecked(n == currentSeverity);
			group->addAction(action);
			++n;
		});

		m_ui.menuLogVerbosityLevel->addSeparator();
		m_ui.menuLogVerbosityLevel->addAction(m_ui.actionTestLogColors);
	}

	void CreateCollectionsMenu()
	{
		m_collectionController->RegisterObserver(this);

		m_ui.menuSelectCollection->clear();

		const auto activeCollection = m_collectionController->GetActiveCollection();
		if (!activeCollection)
			return;

		for (const auto & collection : m_collectionController->GetCollections())
		{
			const auto active = collection->id == activeCollection->id;
			auto * action = m_ui.menuSelectCollection->addAction(collection->name);
			connect(action, &QAction::triggered, &m_self, [&, id = collection->id]
			{
				m_collectionController->SetActiveCollection(id);
			});
			action->setCheckable(true);
			action->setChecked(active);
			action->setEnabled(!active);
		}

		const auto enabled = !m_ui.menuSelectCollection->isEmpty();
		m_ui.actionRemoveCollection->setEnabled(enabled);
		m_ui.menuSelectCollection->setEnabled(enabled);
	}

	void CheckForUpdates(const bool force) const
	{
		auto updateChecker = ILogicFactory::Lock(m_logicFactory)->CreateUpdateChecker();
		updateChecker->CheckForUpdate(force, [updateChecker] () mutable
		{
			updateChecker.reset();
		});
	}

	template<typename T>
	static void OnObjectVisibleChanged(T * obj, void(T::* f)(bool), QAction * show, QAction * hide, const bool value)
	{
		((*obj).*f)(value);
		hide->setVisible(value);
		show->setVisible(!value);
	}

	template<typename T>
	void ConnectShowHide(T * obj, void(T::* f)(bool), QAction * show, QAction * hide, const char * key)
	{
		const auto showHide = [=, &settings = *m_settings] (const bool value)
		{
			settings.Set(key, value);
			Impl::OnObjectVisibleChanged<T>(obj, f, show, hide, value);
		};

		connect(show, &QAction::triggered, &m_self, [=]
		{
			showHide(true);
		});

		connect(hide, &QAction::triggered, &m_self, [=]
		{
			showHide(false);
		});
	}

	static void Reboot()
	{
		QTimer::singleShot(0, []
		{
			QCoreApplication::exit(Constant::RESTART_APP);
		});
	}

private:
	MainWindow & m_self;
	Ui::MainWindow m_ui {};
	std::weak_ptr<const ILogicFactory> m_logicFactory;
	std::shared_ptr<const IUiFactory> m_uiFactory;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
	PropagateConstPtr<IParentWidgetProvider, std::shared_ptr> m_parentWidgetProvider;
	PropagateConstPtr<AnnotationWidget, std::shared_ptr> m_annotationWidget;
	PropagateConstPtr<LocaleController, std::shared_ptr> m_localeController;
	PropagateConstPtr<ILogController, std::shared_ptr> m_logController;
	PropagateConstPtr<QWidget, std::shared_ptr> m_progressBar;
	PropagateConstPtr<QStyledItemDelegate, std::shared_ptr> m_logItemDelegate;
	PropagateConstPtr<ILineOption, std::shared_ptr> m_lineOption;

	PropagateConstPtr<TreeView, std::shared_ptr> m_booksWidget;
	PropagateConstPtr<TreeView, std::shared_ptr> m_navigationWidget;

	QActionGroup * m_themeActionGroup;
	Util::FunctorExecutionForwarder m_forwarder;
	const Log::LogAppender m_logAppender { this };
};

MainWindow::MainWindow(const std::shared_ptr<const ILogicFactory>& logicFactory
	, std::shared_ptr<IUiFactory> uiFactory
	, std::shared_ptr<ISettings> settings
	, std::shared_ptr<ICollectionController> collectionController
	, std::shared_ptr<ICollectionUpdateChecker> collectionUpdateChecker
	, std::shared_ptr<IParentWidgetProvider> parentWidgetProvider
	, std::shared_ptr<AnnotationWidget> annotationWidget
	, std::shared_ptr<LocaleController> localeController
	, std::shared_ptr<ILogController> logController
	, std::shared_ptr<ProgressBar> progressBar
	, std::shared_ptr<LogItemDelegate> logItemDelegate
	, std::shared_ptr<ICommandLine> commandLine
	, std::shared_ptr<ILineOption> lineOption
	, std::shared_ptr<IDatabaseChecker> databaseChecker
	, QWidget * parent
)
	: QMainWindow(parent)
	, m_impl(*this
		, logicFactory
		, std::move(uiFactory)
		, std::move(settings)
		, std::move(collectionController)
		, std::move(collectionUpdateChecker)
		, std::move(parentWidgetProvider)
		, std::move(annotationWidget)
		, std::move(localeController)
		, std::move(logController)
		, std::move(progressBar)
		, std::move(logItemDelegate)
		, std::move(commandLine)
		, std::move(lineOption)
		, std::move(databaseChecker)
	)
{
	PLOGD << "MainWindow created";
}

MainWindow::~MainWindow()
{
	PLOGD << "MainWindow destroyed";
}

void MainWindow::Show()
{
	show();
}
