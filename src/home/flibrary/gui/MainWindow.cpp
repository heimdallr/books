#include "ui_MainWindow.h"

#include "MainWindow.h"

#include <QActionGroup>
#include <QDirIterator>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QStyleFactory>
#include <QTimer>

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"
#include "interface/constants/ObjectConnectionID.h"
#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IBookSearchController.h"
#include "interface/logic/IInpxGenerator.h"
#include "interface/logic/IScriptController.h"
#include "interface/logic/ITreeViewController.h"
#include "interface/logic/IUpdateChecker.h"
#include "interface/logic/IUserDataController.h"
#include "interface/ui/dialogs/IScriptDialog.h"

#include "GuiUtil/GeometryRestorable.h"
#include "GuiUtil/util.h"
#include "logging/LogAppender.h"
#include "util/DyLib.h"
#include "util/FunctorExecutionForwarder.h"
#include "util/ObjectsConnector.h"
#include "util/serializer/Font.h"

#include "TreeView.h"
#include "log.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto MAIN_WINDOW = "MainWindow";
constexpr auto CONTEXT = MAIN_WINDOW;
constexpr auto FONT_DIALOG_TITLE = QT_TRANSLATE_NOOP("MainWindow", "Select font");
constexpr auto CONFIRM_RESTORE_DEFAULT_SETTINGS = QT_TRANSLATE_NOOP("MainWindow", "Are you sure you want to return to default settings?");
constexpr auto CONFIRM_REMOVE_ALL_THEMES = QT_TRANSLATE_NOOP("MainWindow", "Are you sure you want to delete all themes?");
constexpr auto DATABASE_BROKEN = QT_TRANSLATE_NOOP("MainWindow", "Database file \"%1\" is probably corrupted");
constexpr auto DENY_DESTRUCTIVE_OPERATIONS_MESSAGE = QT_TRANSLATE_NOOP("MainWindow", "The right decision!");
constexpr auto ALLOW_DESTRUCTIVE_OPERATIONS_MESSAGE = QT_TRANSLATE_NOOP("MainWindow", "Well, you only have yourself to blame!");
constexpr auto SELECT_QSS_FILE = QT_TRANSLATE_NOOP("MainWindow", "Select stylesheet files");
constexpr auto QSS_FILE_FILTER = QT_TRANSLATE_NOOP("MainWindow", "Qt stylesheet files (*.%1 *.dll);;All files (*.*)");
constexpr const char* ALLOW_DESTRUCTIVE_OPERATIONS_CONFIRMS[] {
	QT_TRANSLATE_NOOP("MainWindow", "By allowing destructive operations, you assume responsibility for the possible loss of books you need. Are you sure?"),
	QT_TRANSLATE_NOOP("MainWindow", "Are you really sure?"),
};

TR_DEF

constexpr auto LOG_SEVERITY_KEY = "ui/LogSeverity";
constexpr auto SHOW_ANNOTATION_KEY = "ui/View/Annotation";
constexpr auto SHOW_ANNOTATION_CONTENT_KEY = "ui/View/AnnotationContent";
constexpr auto SHOW_ANNOTATION_COVER_KEY = "ui/View/AnnotationCover";
constexpr auto SHOW_ANNOTATION_COVER_BUTTONS_KEY = "ui/View/AnnotationCoverButtons";
constexpr auto SHOW_REMOVED_BOOKS_KEY = "ui/View/RemovedBooks";
constexpr auto SHOW_STATUS_BAR_KEY = "ui/View/Status";
constexpr auto SHOW_JOKES_KEY = "ui/View/ShowJokes";
constexpr auto SHOW_SEARCH_BOOK_KEY = "ui/View/ShowSearchBook";
constexpr auto CHECK_FOR_UPDATE_ON_START_KEY = "ui/View/CheckForUpdateOnStart";
constexpr auto QSS = "qss";

class LineEditPlaceholderTextController final : public QObject
{
public:
	LineEditPlaceholderTextController(MainWindow& mainWindow, QLineEdit& lineEdit, QString placeholderText)
		: QObject(&lineEdit)
		, m_mainWindow { mainWindow }
		, m_lineEdit { lineEdit }
		, m_placeholderText { std::move(placeholderText) }
	{
		m_lineEdit.setPlaceholderText(m_placeholderText);
	}

private: // QObject
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		if (event->type() == QEvent::Enter)
			m_lineEdit.setPlaceholderText(m_placeholderText);
		else if (event->type() == QEvent::Leave)
			m_lineEdit.setPlaceholderText({});
		else if (event->type() == QEvent::Show)
			emit m_mainWindow.BookTitleToSearchVisibleChanged();
		return QObject::eventFilter(watched, event);
	}

private:
	MainWindow& m_mainWindow;
	QLineEdit& m_lineEdit;
	const QString m_placeholderText;
};

std::set<QString> GetQssList()
{
	std::set<QString> list;
	QDirIterator it(":/", QStringList() << "*.qss", QDir::Files, QDirIterator::Subdirectories);
	while (it.hasNext())
		list.emplace(it.next());
	return list;
}

} // namespace

class MainWindow::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
	, ICollectionsObserver
	, ILineOption::IObserver
	, virtual plog::IAppender
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(MainWindow& self,
	     const std::shared_ptr<const ILogicFactory>& logicFactory,
	     std::shared_ptr<const IStyleApplierFactory> styleApplierFactory,
	     std::shared_ptr<const IUiFactory> uiFactory,
	     std::shared_ptr<ISettings> settings,
	     std::shared_ptr<ICollectionController> collectionController,
	     std::shared_ptr<const ICollectionUpdateChecker> collectionUpdateChecker,
	     std::shared_ptr<IParentWidgetProvider> parentWidgetProvider,
	     std::shared_ptr<IAnnotationController> annotationController,
	     std::shared_ptr<AnnotationWidget> annotationWidget,
	     std::shared_ptr<LocaleController> localeController,
	     std::shared_ptr<ILogController> logController,
	     std::shared_ptr<QWidget> progressBar,
	     std::shared_ptr<QStyledItemDelegate> logItemDelegate,
	     std::shared_ptr<ICommandLine> commandLine,
	     std::shared_ptr<ILineOption> lineOption,
	     std::shared_ptr<IDatabaseChecker> databaseChecker)
		: GeometryRestorable(*this, settings, MAIN_WINDOW)
		, GeometryRestorableObserver(self)
		, m_self { self }
		, m_logicFactory { logicFactory }
		, m_styleApplierFactory { std::move(styleApplierFactory) }
		, m_uiFactory { std::move(uiFactory) }
		, m_settings { std::move(settings) }
		, m_collectionController { std::move(collectionController) }
		, m_parentWidgetProvider { std::move(parentWidgetProvider) }
		, m_annotationController { std::move(annotationController) }
		, m_annotationWidget { std::move(annotationWidget) }
		, m_localeController { std::move(localeController) }
		, m_logController { std::move(logController) }
		, m_progressBar { std::move(progressBar) }
		, m_logItemDelegate { std::move(logItemDelegate) }
		, m_lineOption { std::move(lineOption) }
		, m_booksWidget { m_uiFactory->CreateTreeViewWidget(ItemType::Books) }
		, m_navigationWidget { m_uiFactory->CreateTreeViewWidget(ItemType::Navigation) }
	{
		Setup();
		ConnectActions();
		CreateStylesMenu();
		CreateLogMenu();
		CreateCollectionsMenu();
		Init();
		StartDelayed(
			[&, commandLine = std::move(commandLine), collectionUpdateChecker = std::move(collectionUpdateChecker), databaseChecker = std::move(databaseChecker)]() mutable
			{
				if (m_collectionController->IsEmpty() || !commandLine->GetInpxDir().empty())
				{
					if (!m_ui.actionShowLog->isChecked())
						m_ui.actionShowLog->trigger();
					return m_collectionController->AddCollection(commandLine->GetInpxDir());
				}

				if (!databaseChecker->IsDatabaseValid())
				{
					m_uiFactory->ShowWarning(Tr(DATABASE_BROKEN).arg(m_collectionController->GetActiveCollection().database));
					return QCoreApplication::exit(Constant::APP_FAILED);
				}

				auto& collectionUpdateCheckerRef = *collectionUpdateChecker;
				collectionUpdateCheckerRef.CheckForUpdate([collectionUpdateChecker = std::move(collectionUpdateChecker)](bool) mutable { collectionUpdateChecker.reset(); });
			});

		if (m_checkForUpdateOnStartEnabled)
			CheckForUpdates(false);
	}

	~Impl() override
	{
		m_collectionController->UnregisterObserver(this);
	}

	void OnBooksSearchFilterValueGeometryChanged(const QRect& geometry) const
	{
		const auto rect = Util::GetGlobalGeometry(*m_ui.lineEditBookTitleToSearch);
		const auto spacerNewWidth = m_searchBooksByTitleLeft->geometry().width() + geometry.x() - rect.x();
		m_searchBooksByTitleLeft->changeSize(std::max(spacerNewWidth, 0), geometry.height(), QSizePolicy::Fixed, QSizePolicy::Expanding);
		const auto lineEditBookTitleToSearchNewWidth = geometry.size().width() + std::min(spacerNewWidth, 0);
		m_ui.lineEditBookTitleToSearch->setMinimumWidth(lineEditBookTitleToSearchNewWidth);
		m_ui.lineEditBookTitleToSearch->setMaximumWidth(lineEditBookTitleToSearchNewWidth);
		m_searchBooksByTitleLayout->invalidate();
	}

	void RemoveCustomStyleFile()
	{
		if (m_lastStyleFileHovered.isEmpty())
			return;

		auto list = m_settings->Get(IStyleApplier::THEME_FILES_KEY).toStringList();
		if (!erase_if(list, [this](const auto& item) { return item == m_lastStyleFileHovered; }))
			return;

		m_settings->Set(IStyleApplier::THEME_FILES_KEY, list);

		auto actions = m_ui.menuTheme->actions();
		if (const auto it = std::ranges::find(actions, m_lastStyleFileHovered, [](const QAction* action) { return action->property(IStyleApplier::THEME_FILE_KEY).toString(); }); it != actions.end())
		{
			m_ui.menuTheme->removeAction(*it);
			if (auto* menu = (*it)->menu<>())
				menu->close();
		}

		if (m_styleApplierFactory->CreateThemeApplier()->GetChecked().second != m_lastStyleFileHovered)
			return;

		m_styleApplierFactory->CreateStyleApplier(IStyleApplier::Type::PluginStyle)->Apply(IStyleApplier::THEME_NAME_DEFAULT, {});
		RebootDialog();
	}

private: // ICollectionsObserver
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
	void write(const plog::Record& record) override
	{
		if (m_ui.statusBar && m_ui.statusBar->isVisible())
			m_forwarder.Forward([&, message = QString(record.getMessage())] { m_ui.statusBar->showMessage(message, 2000); });
	}

private: // ILineOption::IObserver
	void OnOptionEditing(const QString& value) override
	{
		const auto books = ILogicFactory::GetExtractedBooks(m_booksWidget->GetView()->model(), m_booksWidget->GetView()->currentIndex());
		if (books.empty())
			return;

		auto scriptTemplate = value;
		ILogicFactory::FillScriptTemplate(scriptTemplate, books.front());
		PLOGI << scriptTemplate;
	}

	void OnOptionEditingFinished(const QString& /*value*/) override
	{
		disconnect(m_settingsLineEditExecuteContextMenuConnection);
		QTimer::singleShot(0, [&] { m_lineOption->Unregister(this); });
	}

private:
	void Setup()
	{
		PLOGV << "Setup";
		m_ui.setupUi(&m_self);

		m_self.setWindowTitle(QString("%1 %2").arg(PRODUCT_ID, PRODUCT_VERSION));

		m_parentWidgetProvider->SetWidget(&m_self);

		m_delayStarter.setSingleShot(true);
		m_delayStarter.setInterval(std::chrono::milliseconds(200));

		m_ui.navigationWidget->layout()->addWidget(m_navigationWidget.get());
		m_ui.annotationWidget->layout()->addWidget(m_annotationWidget.get());
		m_ui.booksWidget->layout()->addWidget(m_booksWidget.get());
		m_ui.booksWidget->layout()->addWidget(m_progressBar.get());

		if (m_localeController->Setup(*m_ui.menuLanguage); m_ui.menuLanguage->actions().size() < 2)
			delete m_ui.menuLanguage;

		m_ui.logView->setModel(m_logController->GetModel());
		m_ui.logView->setItemDelegate(m_logItemDelegate.get());

		m_ui.settingsLineEdit->setVisible(false);
		m_lineOption->SetLineEdit(m_ui.settingsLineEdit);

		m_ui.lineEditBookTitleToSearch->addAction(m_ui.actionSearchBookByTitle, QLineEdit::LeadingPosition);

		if (!m_collectionController->ActiveCollectionExists())
			m_ui.actionAllowDestructiveOperations->setVisible(false);
		else
			m_ui.actionAllowDestructiveOperations->setChecked(m_collectionController->GetActiveCollection().destructiveOperationsAllowed);

		if (const auto severity = m_settings->Get(LOG_SEVERITY_KEY); severity.isValid())
			m_logController->SetSeverity(severity.toInt());

		if (m_collectionController->ActiveCollectionExists())
			m_self.setWindowTitle(QString("%1 %2 - %3").arg(PRODUCT_ID, PRODUCT_VERSION, m_collectionController->GetActiveCollection().name));

		ReplaceMenuBar();
	}

	void ReplaceMenuBar()
	{
		PLOGV << "ReplaceMenuBar";
		m_ui.lineEditBookTitleToSearch->installEventFilter(new LineEditPlaceholderTextController(m_self, *m_ui.lineEditBookTitleToSearch, Loc::Tr(Loc::Ctx::COMMON, Loc::SEARCH_BOOKS_BY_TITLE_PLACEHOLDER)));
		auto* menuBar = new QWidget(&m_self);
		m_searchBooksByTitleLayout = new QHBoxLayout(menuBar);
		m_self.menuBar()->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
		m_searchBooksByTitleLayout->addWidget(m_self.menuBar());
		m_searchBooksByTitleLayout->addItem((m_searchBooksByTitleLeft = new QSpacerItem(72, 20, QSizePolicy::Fixed)));
		m_searchBooksByTitleLayout->addWidget(m_ui.lineEditBookTitleToSearch);
		m_searchBooksByTitleLayout->addItem(new QSpacerItem(72, 20, QSizePolicy::Expanding));
		m_searchBooksByTitleLayout->setContentsMargins(0, 0, 0, 0);
		m_self.setMenuWidget(menuBar);
	}

	void AllowDestructiveOperation(const bool value)
	{
		PLOGV << "AllowDestructiveOperation";
		if (!m_collectionController->ActiveCollectionExists())
			return;

		if (!value)
		{
			m_collectionController->AllowDestructiveOperation(value);
			if (m_self.isVisible())
				m_uiFactory->ShowInfo(Tr(DENY_DESTRUCTIVE_OPERATIONS_MESSAGE));
			return;
		}

		if (m_self.isVisible())
		{
			if (std::ranges::any_of(ALLOW_DESTRUCTIVE_OPERATIONS_CONFIRMS,
			                        [&](const char* message) { return m_uiFactory->ShowWarning(Tr(message), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes; }))
			{
				m_ui.actionAllowDestructiveOperations->setChecked(false);
				m_uiFactory->ShowInfo(Tr(DENY_DESTRUCTIVE_OPERATIONS_MESSAGE));
				return;
			}

			m_uiFactory->ShowInfo(Tr(ALLOW_DESTRUCTIVE_OPERATIONS_MESSAGE));
		}

		m_collectionController->AllowDestructiveOperation(value);
	}

	void ConnectActionsFile()
	{
		PLOGV << "ConnectActionsFile";
		const auto userDataOperation = [this](const auto& operation)
		{
			auto controller = ILogicFactory::Lock(m_logicFactory)->CreateUserDataController();
			std::invoke(operation, *controller, [controller]() mutable { controller.reset(); });
		};
		connect(m_ui.actionExportUserData, &QAction::triggered, &m_self, [=] { userDataOperation(&IUserDataController::Backup); });
		connect(m_ui.actionImportUserData, &QAction::triggered, &m_self, [=] { userDataOperation(&IUserDataController::Restore); });
		connect(m_ui.actionExit, &QAction::triggered, &m_self, [] { QCoreApplication::exit(); });
	}

	void ConnectActionsCollection()
	{
		PLOGV << "ConnectActionsCollection";
		connect(m_ui.actionAddNewCollection, &QAction::triggered, &m_self, [&] { m_collectionController->AddCollection({}); });
		connect(m_ui.actionRemoveCollection, &QAction::triggered, &m_self, [&] { m_collectionController->RemoveCollection(); });
		connect(m_ui.actionGenerateIndexInpx, &QAction::triggered, &m_self, [&] { GenerateCollectionInpx(); });
		connect(m_ui.actionShowCollectionCleaner, &QAction::triggered, &m_self, [&] { m_uiFactory->CreateCollectionCleaner()->exec(); });
		ConnectSettings(m_ui.actionAllowDestructiveOperations, {}, this, &Impl::AllowDestructiveOperation);
	}

	void ConnectActionsSettingsAnnotation()
	{
		PLOGV << "ConnectActionsSettingsAnnotation";
		ConnectSettings(m_ui.actionShowAnnotationCover, SHOW_ANNOTATION_COVER_KEY, m_annotationWidget.get(), &AnnotationWidget::ShowCover);
		ConnectSettings(m_ui.actionShowAnnotationContent, SHOW_ANNOTATION_CONTENT_KEY, m_annotationWidget.get(), &AnnotationWidget::ShowContent);
		ConnectSettings(m_ui.actionShowAnnotationCoverButtons, SHOW_ANNOTATION_COVER_BUTTONS_KEY, m_annotationWidget.get(), &AnnotationWidget::ShowCoverButtons);
		ConnectSettings(m_ui.actionShowJokes, SHOW_JOKES_KEY, m_annotationController.get(), &IAnnotationController::ShowJokes);
		connect(m_ui.actionHideAnnotation, &QAction::visibleChanged, &m_self, [&] { m_ui.menuAnnotation->menuAction()->setVisible(m_ui.actionHideAnnotation->isVisible()); });
	}

	void ConnectActionsSettingsFont()
	{
		PLOGV << "ConnectActionsSettingsFont";
		const auto incrementFontSize = [&](const int value)
		{
			const auto fontSize = m_settings->Get(Constant::Settings::FONT_SIZE_KEY, Constant::Settings::FONT_SIZE_DEFAULT);
			m_settings->Set(Constant::Settings::FONT_SIZE_KEY, fontSize + value);
		};
		connect(m_ui.actionFontSizeUp, &QAction::triggered, &m_self, [=] { incrementFontSize(1); });
		connect(m_ui.actionFontSizeDown, &QAction::triggered, &m_self, [=] { incrementFontSize(-1); });
		connect(m_ui.actionFontSettings,
		        &QAction::triggered,
		        &m_self,
		        [&]
		        {
					if (const auto font = m_uiFactory->GetFont(Tr(FONT_DIALOG_TITLE), m_self.font()))
					{
						const SettingsGroup group(*m_settings, Constant::Settings::FONT_KEY);
						Util::Serialize(*font, *m_settings);
					}
				});
	}

	void ConnectActionsSettingsLog()
	{
		PLOGV << "ConnectActionsSettingsLog";
		connect(m_ui.logView, &QWidget::customContextMenuRequested, &m_self, [&](const QPoint& pos) { m_ui.menuLog->exec(m_ui.logView->viewport()->mapToGlobal(pos)); });
		connect(m_ui.actionShowLog, &QAction::triggered, &m_self, [&](const bool checked) { m_ui.stackedWidget->setCurrentIndex(checked ? 1 : 0); });
		connect(m_ui.actionClearLog, &QAction::triggered, &m_self, [&] { m_logController->Clear(); });
		const auto logAction = [this](const auto& action)
		{
			m_logController->ShowCollectionStatistics();
			if (!m_ui.actionShowLog->isChecked())
				m_ui.actionShowLog->trigger();
			std::invoke(action, *m_logController);
		};
		connect(m_ui.actionShowCollectionStatistics, &QAction::triggered, &m_self, [=] { logAction(&ILogController::ShowCollectionStatistics); });
		connect(m_ui.actionTestLogColors, &QAction::triggered, &m_self, [=] { logAction(&ILogController::TestColors); });
	}

	void ConnectActionsSettingsTheme()
	{
		PLOGV << "ConnectActionsSettingsTheme";
		connect(m_ui.actionAddThemes, &QAction::triggered, &m_self, [this] { OpenExternalStyle(m_uiFactory->GetOpenFileNames(QSS, Tr(SELECT_QSS_FILE), Tr(QSS_FILE_FILTER).arg(QSS))); });
		connect(m_ui.actionDeleteAllThemes, &QAction::triggered, &m_self, [this] { DeleteCustomThemes(); });
	}

	void ConnectActionsSettingsView()
	{
		PLOGV << "ConnectActionsSettingsView";
		ConnectSettings(m_ui.actionShowRemoved, SHOW_REMOVED_BOOKS_KEY, m_booksWidget.get(), &TreeView::ShowRemoved);
		ConnectSettings(m_ui.actionShowStatusBar, SHOW_STATUS_BAR_KEY, qobject_cast<QWidget*>(m_ui.statusBar), &QWidget::setVisible);
		ConnectSettings(m_ui.actionShowSearchBookString, SHOW_SEARCH_BOOK_KEY, qobject_cast<QWidget*>(m_ui.lineEditBookTitleToSearch), &QWidget::setVisible);
		ConnectShowHide(m_ui.annotationWidget, &QWidget::setVisible, m_ui.actionShowAnnotation, m_ui.actionHideAnnotation, SHOW_ANNOTATION_KEY);

		auto restoreDefaultSettings = [this]
		{
			if (m_uiFactory->ShowQuestion(Tr(CONFIRM_RESTORE_DEFAULT_SETTINGS), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
				return;

			m_settings->Remove("ui");
			Reboot();
		};
		connect(m_ui.actionRestoreDefaultSettings, &QAction::triggered, &m_self, [restoreDefaultSettings = std::move(restoreDefaultSettings)] { restoreDefaultSettings(); });

		ConnectActionsSettingsAnnotation();
		ConnectActionsSettingsFont();
		ConnectActionsSettingsLog();
		ConnectActionsSettingsTheme();
	}

	void ConnectActionsSettings()
	{
		PLOGV << "ConnectActionsSettings";
		ConnectActionsSettingsView();

		connect(m_localeController.get(), &LocaleController::LocaleChanged, &m_self, [&] { Reboot(); });
		connect(m_ui.actionScripts, &QAction::triggered, &m_self, [&] { m_uiFactory->CreateScriptDialog()->Exec(); });
		connect(m_ui.actionOpds,
		        &QAction::triggered,
		        &m_self,
		        [&]
		        {
					try
					{
						m_uiFactory->CreateOpdsDialog()->exec();
					}
					catch (const std::exception& ex)
					{
						m_uiFactory->ShowError(QString::fromStdString(ex.what()));
					}
				});
		connect(m_ui.actionExportTemplate,
		        &QAction::triggered,
		        &m_self,
		        [&]
		        {
					m_settingsLineEditExecuteContextMenuConnection = connect(m_ui.settingsLineEdit,
			                                                                 &QWidget::customContextMenuRequested,
			                                                                 &m_self,
			                                                                 [this]
			                                                                 {
																				 {
																					 QSignalBlocker blocker(m_ui.settingsLineEdit);
																					 IScriptController::ExecuteContextMenu(m_ui.settingsLineEdit);
																				 }
																				 emit m_ui.settingsLineEdit->textChanged(m_ui.settingsLineEdit->text());
																			 });
					m_lineOption->Register(this);
					m_lineOption->SetSettingsKey(Constant::Settings::EXPORT_TEMPLATE_KEY, IScriptController::GetDefaultOutputFileNameTemplate());
				});

		ConnectSettings(m_ui.actionPermanentLanguageFilter, Constant::Settings::KEEP_RECENT_LANG_FILTER_KEY);
	}

	void ConnectActionsHelp()
	{
		PLOGV << "ConnectActionsHelp";
		connect(m_ui.actionCheckForUpdates, &QAction::triggered, &m_self, [&] { CheckForUpdates(true); });
		connect(m_ui.actionAbout, &QAction::triggered, &m_self, [&] { m_uiFactory->ShowAbout(); });
		ConnectSettings(m_ui.actionEnableCheckForUpdateOnStart, CHECK_FOR_UPDATE_ON_START_KEY, this, &Impl::SetCheckForUpdateOnStartEnabled);
	}

	void ConnectActions()
	{
		PLOGV << "ConnectActions";
		ConnectActionsFile();
		ConnectActionsCollection();
		ConnectActionsSettings();
		ConnectActionsHelp();

		connect(m_navigationWidget.get(), &TreeView::NavigationModeNameChanged, m_booksWidget.get(), &TreeView::SetNavigationModeName);
		connect(m_ui.lineEditBookTitleToSearch, &QLineEdit::returnPressed, &m_self, [this] { SearchBookByTitle(); });
		connect(m_ui.actionSearchBookByTitle, &QAction::triggered, &m_self, [this] { SearchBookByTitle(); });
	}

	template <typename T = QAction>
	void ConnectSettings(QAction* action, QString key, T* obj = nullptr, void (T::*f)(bool) = nullptr)
	{
		if (!key.isEmpty())
			action->setChecked(m_settings->Get(key, action->isChecked()));
		auto invoker = [obj, f](const bool checked)
		{
			if (obj && f)
				std::invoke(f, obj, checked);
		};
		invoker(action->isChecked());

		connect(action,
		        &QAction::triggered,
		        &m_self,
		        [this, invoker = std::move(invoker), key = std::move(key)](const bool checked)
		        {
					if (!key.isEmpty())
						m_settings->Set(key, checked);
					invoker(checked);
				});
	}

	template <typename T>
	void ConnectShowHide(T* obj, void (T::*f)(bool), QAction* show, QAction* hide, const char* key = nullptr)
	{
		auto apply = [=](const bool value)
		{
			std::invoke(f, obj, value);
			hide->setVisible(value);
			show->setVisible(!value);
		};

		if (key)
		{
			const auto value = m_settings->Get(key).toBool();
			apply(value);
		}

		const auto showHide = [&settings = *m_settings, key, apply = std::move(apply)](const bool value)
		{
			if (key)
				settings.Set(key, value);
			apply(value);
		};

		connect(show, &QAction::triggered, &m_self, [=] { showHide(true); });
		connect(hide, &QAction::triggered, &m_self, [=] { showHide(false); });
	}

	void ApplyStyleAction(QAction& action, const QActionGroup& group) const
	{
		PLOGV << "ApplyStyleAction";
		for (auto* groupAction : group.actions())
			groupAction->setEnabled(true);

		action.setEnabled(false);

		auto applier = m_styleApplierFactory->CreateStyleApplier(static_cast<IStyleApplier::Type>(action.property(IStyleApplier::THEME_TYPE_KEY).toInt()));
		applier->Apply(action.property(IStyleApplier::THEME_NAME_KEY).toString(), action.property(IStyleApplier::THEME_FILE_KEY).toString());
		RebootDialog();
	}

	void RebootDialog() const
	{
		if (m_uiFactory->ShowQuestion(Loc::Tr(Loc::Ctx::COMMON, Loc::CONFIRM_RESTART), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
			Reboot();
	}

	void AddStyleActionsToGroup(const std::vector<QAction*>& actions, QActionGroup* group)
	{
		for (auto* action : actions)
		{
			group->addAction(action);
			connect(action, &QAction::triggered, &m_self, [this, action, group] { ApplyStyleAction(*action, *group); });
		}
	}

	QAction* CreateStyleAction(QMenu& menu, const IStyleApplier::Type type, const QString& actionName, const QString& name, const QString& file = {})
	{
		auto* action = menu.addAction(QFileInfo(actionName).completeBaseName());

		action->setProperty(IStyleApplier::THEME_NAME_KEY, name);
		action->setProperty(IStyleApplier::THEME_TYPE_KEY, static_cast<int>(type));
		action->setProperty(IStyleApplier::THEME_FILE_KEY, file);
		action->setCheckable(true);

		connect(action, &QAction::hovered, &m_self, [this, file] { m_lastStyleFileHovered = file; });

		return action;
	}

	void CreateStylesMenu()
	{
		PLOGV << "CreateStylesMenu";
		const auto addActionGroup = [this](const std::vector<QAction*>& actions, QActionGroup* group)
		{
			AddStyleActionsToGroup(actions, group);
			m_styleApplierFactory->CheckAction(actions);
		};
		addActionGroup({ m_ui.actionColorSchemeSystem, m_ui.actionColorSchemeLight, m_ui.actionColorSchemeDark }, new QActionGroup(&m_self));

		std::vector<QAction*> styles;
		for (const auto& key : QStyleFactory::keys())
			if (const auto* style = QStyleFactory::create(key))
				styles.emplace_back(CreateStyleAction(*m_ui.menuTheme, IStyleApplier::Type::PluginStyle, style->name(), key));

		m_ui.menuTheme->addSeparator();

		if (const auto externalThemesVar = m_settings->Get(IStyleApplier::THEME_FILES_KEY); externalThemesVar.isValid())
		{
			auto externalThemes = externalThemesVar.toStringList();
			std::ranges::sort(externalThemes);
			for (const auto& fileName : externalThemes)
				std::ranges::copy(AddExternalStyle(fileName), std::back_inserter(styles));
		}
		addActionGroup(styles, m_stylesActionGroup);

		m_ui.menuTheme->addSeparator();
		m_ui.menuTheme->addAction(m_ui.actionAddThemes);
		m_ui.menuTheme->addAction(m_ui.actionDeleteAllThemes);
	}

	std::vector<QAction*> AddExternalStyle(const QString& fileName)
	{
		assert(!fileName.isEmpty());

		const QFileInfo fileInfo(fileName);
		if (!fileInfo.exists())
			return {};

		const auto ext = fileInfo.suffix().toLower();
		if (ext == "qss")
			return { CreateStyleAction(*m_ui.menuTheme, IStyleApplier::Type::QssStyle, fileName, fileInfo.completeBaseName(), fileName) };

		if (ext == "dll")
			return AddEternalStyleDll(fileInfo);

		return {};
	}

	std::vector<QAction*> AddEternalStyleDll(const QFileInfo& fileInfo)
	{
		const auto addLibList = [&](const std::set<QString>& libList) -> std::vector<QAction*>
		{
			if (libList.empty())
				return {};

			auto* menu = m_ui.menuTheme->addMenu(fileInfo.completeBaseName());
			menu->setFont(m_self.font());

			auto* action = menu->menuAction();
			action->setProperty(IStyleApplier::THEME_FILE_KEY, fileInfo.filePath());
			connect(action, &QAction::hovered, &m_self, [this, file = fileInfo.filePath()] { m_lastStyleFileHovered = file; });

			std::vector<QAction*> result;
			result.reserve(libList.size());
			std::ranges::transform(libList, std::back_inserter(result), [&](const auto& qss) { return CreateStyleAction(*menu, IStyleApplier::Type::DllStyle, qss, qss, fileInfo.filePath()); });

			return result;
		};

		const auto currentList = GetQssList();

		if (auto applier = m_styleApplierFactory->CreateThemeApplier(); applier->GetType() == IStyleApplier::Type::DllStyle && applier->GetChecked().second == fileInfo.filePath())
		{
			auto libList = currentList;
			libList.erase(IStyleApplier::STYLE_FILE_NAME);
			return addLibList(libList);
		}

		Util::DyLib lib(fileInfo.filePath().toStdString());
		if (!lib.IsOpen())
		{
			PLOGE << lib.GetErrorDescription();
			return {};
		}

		auto libList = GetQssList();
		erase_if(libList, [&](const auto& item) { return currentList.contains(item); });
		return addLibList(libList);
	}

	void OpenExternalStyle(const QStringList& fileNames)
	{
		auto list = m_settings->Get(IStyleApplier::THEME_FILES_KEY).toStringList();

		for (const auto& fileName : fileNames)
		{
			if (list.contains(fileName, Qt::CaseInsensitive))
				continue;

			auto actions = AddExternalStyle(fileName);
			if (actions.empty())
				continue;

			AddStyleActionsToGroup(actions, m_stylesActionGroup);
			list << fileName;
		}

		m_settings->Set(IStyleApplier::THEME_FILES_KEY, list);
	}

	void DeleteCustomThemes()
	{
		if (m_uiFactory->ShowQuestion(Tr(CONFIRM_REMOVE_ALL_THEMES), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) != QMessageBox::Yes)
			return;

		for (auto* action : m_ui.menuTheme->actions())
		{
			const auto file = action->property(IStyleApplier::THEME_FILE_KEY).toString();
			if (!file.isEmpty())
				m_ui.menuTheme->removeAction(action);
		}

		m_settings->Remove(IStyleApplier::THEME_FILES_KEY);

		if (m_styleApplierFactory->CreateThemeApplier()->GetType() == IStyleApplier::Type::PluginStyle)
			return;

		m_styleApplierFactory->CreateStyleApplier(IStyleApplier::Type::PluginStyle)->Apply(IStyleApplier::THEME_NAME_DEFAULT, {});
		RebootDialog();
	}

	void SearchBookByTitle()
	{
		if (!m_collectionController->ActiveCollectionExists())
			return;

		auto searchString = m_ui.lineEditBookTitleToSearch->text();
		if (searchString.isEmpty())
			return;

		auto searchController = ILogicFactory::Lock(m_logicFactory)->CreateSearchController();
		searchController->Search(searchString,
		                         [this, searchController](const long long id) mutable
		                         {
									 if (id <= 0)
										 return;

									 m_settings->Set(QString(Constant::Settings::RECENT_NAVIGATION_ID_KEY).arg(m_collectionController->GetActiveCollectionId()).arg(Loc::Search), QString::number(id));
									 ILogicFactory::Lock(m_logicFactory)->GetTreeViewController(ItemType::Navigation)->SetMode(Loc::Search);
									 searchController.reset();
								 });
	}

	void CreateLogMenu()
	{
		PLOGV << "CreateLogMenu";
		auto* group = new QActionGroup(&m_self);
		const auto currentSeverity = m_logController->GetSeverity();
		std::ranges::for_each(m_logController->GetSeverities(),
		                      [&, n = 0](const char* name) mutable
		                      {
								  auto* action = m_ui.menuLogVerbosityLevel->addAction(Loc::Tr(Loc::Ctx::LOGGING, name),
			                                                                           [&, n]
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
		PLOGV << "CreateCollectionsMenu";
		m_collectionController->RegisterObserver(this);

		m_ui.menuSelectCollection->clear();

		if (!m_collectionController->ActiveCollectionExists())
			return;

		const auto& activeCollection = m_collectionController->GetActiveCollection();

		auto* group = new QActionGroup(&m_self);
		group->setExclusive(true);

		for (const auto& collection : m_collectionController->GetCollections())
		{
			const auto active = collection->id == activeCollection.id;
			auto* action = m_ui.menuSelectCollection->addAction(collection->name);
			connect(action, &QAction::triggered, &m_self, [&, id = collection->id] { m_collectionController->SetActiveCollection(id); });
			action->setCheckable(true);
			action->setChecked(active);
			action->setEnabled(!active);
			group->addAction(action);
		}

		const auto enabled = !m_ui.menuSelectCollection->isEmpty();
		m_ui.actionRemoveCollection->setEnabled(enabled);
		m_ui.menuSelectCollection->setEnabled(enabled);
	}

	void CheckForUpdates(const bool force) const
	{
		auto updateChecker = ILogicFactory::Lock(m_logicFactory)->CreateUpdateChecker();
		updateChecker->CheckForUpdate(force, [updateChecker]() mutable { updateChecker.reset(); });
	}

	void StartDelayed(std::function<void()> f)
	{
		PLOGV << "StartDelayed";
		connect(&m_delayStarter,
		        &QTimer::timeout,
		        &m_self,
		        [this, f = std::move(f)]
		        {
					m_delayStarter.disconnect();
					f();
				});
		m_delayStarter.start();
	}

	void GenerateCollectionInpx() const
	{
		auto inpxGenerator = ILogicFactory::Lock(m_logicFactory)->CreateInpxGenerator();
		auto& inpxGeneratorRef = *inpxGenerator;
		if (auto inpxFileName = m_uiFactory->GetSaveFileName(Constant::Settings::EXPORT_DIALOG_KEY, Loc::Tr(Loc::EXPORT, Loc::SELECT_INPX_FILE), Loc::Tr(Loc::EXPORT, Loc::SELECT_INPX_FILE_FILTER));
		    !inpxFileName.isEmpty())
			inpxGeneratorRef.GenerateInpx(std::move(inpxFileName), [inpxGenerator = std::move(inpxGenerator)](bool) mutable { inpxGenerator.reset(); });
	}

	void SetCheckForUpdateOnStartEnabled(const bool value) noexcept
	{
		m_checkForUpdateOnStartEnabled = value;
	}

	static void Reboot()
	{
		QTimer::singleShot(0, [] { QCoreApplication::exit(Constant::RESTART_APP); });
	}

private:
	MainWindow& m_self;
	Ui::MainWindow m_ui {};
	QTimer m_delayStarter;
	std::weak_ptr<const ILogicFactory> m_logicFactory;
	std::shared_ptr<const IStyleApplierFactory> m_styleApplierFactory;
	std::shared_ptr<const IUiFactory> m_uiFactory;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
	PropagateConstPtr<IParentWidgetProvider, std::shared_ptr> m_parentWidgetProvider;
	PropagateConstPtr<IAnnotationController, std::shared_ptr> m_annotationController;
	PropagateConstPtr<AnnotationWidget, std::shared_ptr> m_annotationWidget;
	PropagateConstPtr<LocaleController, std::shared_ptr> m_localeController;
	PropagateConstPtr<ILogController, std::shared_ptr> m_logController;
	PropagateConstPtr<QWidget, std::shared_ptr> m_progressBar;
	PropagateConstPtr<QStyledItemDelegate, std::shared_ptr> m_logItemDelegate;
	PropagateConstPtr<ILineOption, std::shared_ptr> m_lineOption;

	PropagateConstPtr<TreeView, std::shared_ptr> m_booksWidget;
	PropagateConstPtr<TreeView, std::shared_ptr> m_navigationWidget;

	Util::FunctorExecutionForwarder m_forwarder;
	const Log::LogAppender m_logAppender { this };

	QMetaObject::Connection m_settingsLineEditExecuteContextMenuConnection;
	QSpacerItem* m_searchBooksByTitleLeft;
	QLayout* m_searchBooksByTitleLayout;

	bool m_checkForUpdateOnStartEnabled { true };

	QActionGroup* m_stylesActionGroup { new QActionGroup(&m_self) };
	QString m_lastStyleFileHovered;
};

MainWindow::MainWindow(const std::shared_ptr<const ILogicFactory>& logicFactory,
                       std::shared_ptr<const IStyleApplierFactory> styleApplierFactory,
                       std::shared_ptr<IUiFactory> uiFactory,
                       std::shared_ptr<ISettings> settings,
                       std::shared_ptr<ICollectionController> collectionController,
                       std::shared_ptr<ICollectionUpdateChecker> collectionUpdateChecker,
                       std::shared_ptr<IParentWidgetProvider> parentWidgetProvider,
                       std::shared_ptr<IAnnotationController> annotationController,
                       std::shared_ptr<AnnotationWidget> annotationWidget,
                       std::shared_ptr<LocaleController> localeController,
                       std::shared_ptr<ILogController> logController,
                       std::shared_ptr<ProgressBar> progressBar,
                       std::shared_ptr<LogItemDelegate> logItemDelegate,
                       std::shared_ptr<ICommandLine> commandLine,
                       std::shared_ptr<ILineOption> lineOption,
                       std::shared_ptr<IDatabaseChecker> databaseChecker,
                       QWidget* parent)
	: QMainWindow(parent)
	, m_impl(*this,
             logicFactory,
             std::move(styleApplierFactory),
             std::move(uiFactory),
             std::move(settings),
             std::move(collectionController),
             std::move(collectionUpdateChecker),
             std::move(parentWidgetProvider),
             std::move(annotationController),
             std::move(annotationWidget),
             std::move(localeController),
             std::move(logController),
             std::move(progressBar),
             std::move(logItemDelegate),
             std::move(commandLine),
             std::move(lineOption),
             std::move(databaseChecker))
{
	Util::ObjectsConnector::registerEmitter(ObjectConnectorID::BOOK_TITLE_TO_SEARCH_VISIBLE_CHANGED, this, SIGNAL(BookTitleToSearchVisibleChanged()));
	Util::ObjectsConnector::registerReceiver(ObjectConnectorID::BOOKS_SEARCH_FILTER_VALUE_GEOMETRY_CHANGED, this, SLOT(OnBooksSearchFilterValueGeometryChanged(const QRect&)), true);
	PLOGV << "MainWindow created";
}

MainWindow::~MainWindow()
{
	PLOGV << "MainWindow destroyed";
}

void MainWindow::Show()
{
	show();
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_Delete)
		m_impl->RemoveCustomStyleFile();

	QMainWindow::keyPressEvent(event);
}

void MainWindow::OnBooksSearchFilterValueGeometryChanged(const QRect& geometry)
{
	m_impl->OnBooksSearchFilterValueGeometryChanged(geometry);
}
