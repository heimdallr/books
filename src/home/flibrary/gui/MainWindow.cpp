#include "ui_MainWindow.h"

#include "MainWindow.h"

#include <ranges>

#include <QActionGroup>
#include <QDesktopServices>
#include <QDirIterator>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QStyleFactory>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolBar>

#include "fnd/IsOneOf.h"

#include "interface/Localization.h"
#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"
#include "interface/constants/ObjectConnectionID.h"
#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IBookSearchController.h"
#include "interface/logic/IInpxGenerator.h"
#include "interface/logic/IOpdsController.h"
#include "interface/logic/IScriptController.h"
#include "interface/logic/IUpdateChecker.h"
#include "interface/logic/IUserDataController.h"
#include "interface/ui/IAlphabetPanel.h"
#include "interface/ui/dialogs/IScriptDialog.h"

#include "gutil/GeometryRestorable.h"
#include "gutil/util.h"
#include "logging/LogAppender.h"
#include "util/DyLib.h"
#include "util/FunctorExecutionForwarder.h"
#include "util/ObjectsConnector.h"
#include "util/serializer/Font.h"

#include "Constant.h"
#include "StackedPage.h"
#include "TreeView.h"
#include "log.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto        MAIN_WINDOW                          = "MainWindow";
constexpr auto        CONTEXT                              = MAIN_WINDOW;
constexpr auto        EXIT                                 = QT_TRANSLATE_NOOP("MainWindow", "Exit");
constexpr auto        OPEN                                 = QT_TRANSLATE_NOOP("MainWindow", "Open FLibrary");
constexpr auto        FONT_DIALOG_TITLE                    = QT_TRANSLATE_NOOP("MainWindow", "Select font");
constexpr auto        CONFIRM_RESTORE_DEFAULT_SETTINGS     = QT_TRANSLATE_NOOP("MainWindow", "Are you sure you want to return to default settings?");
constexpr auto        CONFIRM_REMOVE_ALL_THEMES            = QT_TRANSLATE_NOOP("MainWindow", "Are you sure you want to delete all themes?");
constexpr auto        DATABASE_BROKEN                      = QT_TRANSLATE_NOOP("MainWindow", "Database file \"%1\" is probably corrupted");
constexpr auto        DENY_DESTRUCTIVE_OPERATIONS_MESSAGE  = QT_TRANSLATE_NOOP("MainWindow", "The right decision!");
constexpr auto        ALLOW_DESTRUCTIVE_OPERATIONS_MESSAGE = QT_TRANSLATE_NOOP("MainWindow", "Well, you only have yourself to blame!");
constexpr auto        SELECT_QSS_FILE                      = QT_TRANSLATE_NOOP("MainWindow", "Select stylesheet files");
constexpr auto        QSS_FILE_FILTER                      = QT_TRANSLATE_NOOP("MainWindow", "Qt stylesheet files (*.%1 *.dll);;All files (*.*)");
constexpr auto        SEARCH_BOOKS_BY_TITLE_PLACEHOLDER    = QT_TRANSLATE_NOOP("MainWindow", "To search for books by author, series, or title, enter the name or title here and press Enter");
constexpr auto        ENABLE_ALL                           = QT_TRANSLATE_NOOP("MainWindow", "Enable all");
constexpr auto        DISABLE_ALL                          = QT_TRANSLATE_NOOP("MainWindow", "Disable all");
constexpr auto        MY_FOLDER                            = QT_TRANSLATE_NOOP("MainWindow", "My export folder");
constexpr const char* ALLOW_DESTRUCTIVE_OPERATIONS_CONFIRMS[] {
	QT_TRANSLATE_NOOP("MainWindow", "By allowing destructive operations, you assume responsibility for the possible loss of books you need. Are you sure?"),
	QT_TRANSLATE_NOOP("MainWindow", "Are you really sure?"),
};

TR_DEF

constexpr auto LOG_SEVERITY_KEY                   = "ui/LogSeverity";
constexpr auto SHOW_AUTHOR_ANNOTATION_KEY         = "ui/View/AuthorAnnotation";
constexpr auto SHOW_ANNOTATION_KEY                = "ui/View/Annotation";
constexpr auto SHOW_ANNOTATION_CONTENT_KEY        = "ui/View/AnnotationContent";
constexpr auto SHOW_ANNOTATION_COVER_KEY          = "ui/View/AnnotationCover";
constexpr auto SHOW_ANNOTATION_COVER_BUTTONS_KEY  = "ui/View/AnnotationCoverButtons";
constexpr auto SHOW_ANNOTATION_JOKES_KEY_TEMPLATE = "ui/View/AnnotationJokes/%1";
constexpr auto SHOW_STATUS_BAR_KEY                = "ui/View/Status";
constexpr auto SHOW_REVIEWS_KEY                   = "ui/View/ShowReadersReviews";
constexpr auto SHOW_SEARCH_BOOK_KEY               = "ui/View/ShowSearchBook";
constexpr auto CHECK_FOR_UPDATE_ON_START_KEY      = "ui/View/CheckForUpdateOnStart";
constexpr auto START_FOCUSED_CONTROL              = "ui/View/StartFocusedControl";
constexpr auto QSS                                = "qss";

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
	MainWindow&   m_mainWindow;
	QLineEdit&    m_lineEdit;
	const QString m_placeholderText;
};

std::set<QString> GetQssList()
{
	std::set<QString> list;
	QDirIterator      it(":/", QStringList() << "*.qss", QDir::Files, QDirIterator::Subdirectories);
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
	, IAlphabetPanel::IObserver
	, ITreeViewController::IObserver
	, virtual plog::IAppender
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(
		MainWindow&                                     self,
		const std::shared_ptr<const ILogicFactory>&     logicFactory,
		std::shared_ptr<const IStyleApplierFactory>     styleApplierFactory,
		std::shared_ptr<const IJokeRequesterFactory>    jokeRequesterFactory,
		std::shared_ptr<const IUiFactory>               uiFactory,
		std::shared_ptr<const IDatabaseUser>            databaseUser,
		std::shared_ptr<const ICollectionUpdateChecker> collectionUpdateChecker,
		std::shared_ptr<const IDatabaseChecker>         databaseChecker,
		std::shared_ptr<const ICommandLine>             commandLine,
		std::shared_ptr<ISettings>                      settings,
		std::shared_ptr<ICollectionController>          collectionController,
		std::shared_ptr<IParentWidgetProvider>          parentWidgetProvider,
		std::shared_ptr<IAnnotationController>          annotationController,
		std::shared_ptr<AnnotationWidget>               annotationWidget,
		std::shared_ptr<AuthorAnnotationWidget>         authorAnnotationWidget,
		std::shared_ptr<LocaleController>               localeController,
		std::shared_ptr<ILogController>                 logController,
		std::shared_ptr<QWidget>                        progressBar,
		std::shared_ptr<QStyledItemDelegate>            logItemDelegate,
		std::shared_ptr<ILineOption>                    lineOption,
		std::shared_ptr<IAlphabetPanel>                 alphabetPanel
	)
		: GeometryRestorable(*this, settings, MAIN_WINDOW)
		, GeometryRestorableObserver(self)
		, m_self { self }
		, m_logicFactory { logicFactory }
		, m_styleApplierFactory { std::move(styleApplierFactory) }
		, m_jokeRequesterFactory { std::move(jokeRequesterFactory) }
		, m_uiFactory { std::move(uiFactory) }
		, m_databaseUser { std::move(databaseUser) }
		, m_settings { std::move(settings) }
		, m_collectionController { std::move(collectionController) }
		, m_parentWidgetProvider { std::move(parentWidgetProvider) }
		, m_annotationController { std::move(annotationController) }
		, m_annotationWidget { std::move(annotationWidget) }
		, m_authorAnnotationWidget { std::move(authorAnnotationWidget) }
		, m_localeController { std::move(localeController) }
		, m_logController { std::move(logController) }
		, m_progressBar { std::move(progressBar) }
		, m_logItemDelegate { std::move(logItemDelegate) }
		, m_lineOption { std::move(lineOption) }
		, m_alphabetPanel { std::move(alphabetPanel) }
		, m_navigationViewController { ILogicFactory::Lock(m_logicFactory)->GetTreeViewController(ItemType::Navigation) }
		, m_booksWidget { m_uiFactory->CreateTreeViewWidget(ItemType::Books) }
		, m_navigationWidget { m_uiFactory->CreateTreeViewWidget(ItemType::Navigation) }
	{
		Setup();
		ConnectActions();
		CreateStylesMenu();
		CreateLogMenu();
		CreateCollectionsMenu();
		CreateViewNavigationMenu();
		LoadGeometry();
		StartDelayed([this, commandLine = std::move(commandLine), collectionUpdateChecker = std::move(collectionUpdateChecker), databaseChecker = std::move(databaseChecker)]() mutable {
			if (m_collectionController->IsEmpty() || !commandLine->GetInpxDir().empty())
			{
				m_self.showNormal();
				m_self.raise();
				m_self.activateWindow();

				if (!m_ui.actionShowLog->isChecked())
					m_ui.actionShowLog->trigger();
				return m_collectionController->AddCollection(commandLine->GetInpxDir());
			}

			if (!databaseChecker->IsDatabaseValid())
			{
				m_uiFactory->ShowWarning(Tr(DATABASE_BROKEN).arg(m_collectionController->GetActiveCollection().GetDatabase()));
				return QCoreApplication::exit(Constant::APP_FAILED);
			}

			auto& collectionUpdateCheckerRef = *collectionUpdateChecker;
			collectionUpdateCheckerRef.CheckForUpdate([this, collectionUpdateChecker = std::move(collectionUpdateChecker)](const bool result, const Collection& updatedCollection) mutable {
				if (result)
					m_collectionController->OnInpxUpdateChecked(updatedCollection);

				collectionUpdateChecker.reset();
			});
		});

		if (m_checkForUpdateOnStartEnabled && m_collectionController->ActiveCollectionExists())
			CheckForUpdates(false);
	}

	~Impl() override
	{
		SaveGeometry();
		m_navigationViewController->UnregisterObserver(this);
		m_collectionController->UnregisterObserver(this);
		m_alphabetPanel->UnregisterObserver(this);
	}

	void OnBooksSearchFilterValueGeometryChanged(const QRect& geometry) const
	{
		const auto rect           = Util::GetGlobalGeometry(*m_ui.lineEditBookTitleToSearch);
		const auto spacerNewWidth = m_searchBooksByTitleLeft->geometry().width() + geometry.x() - rect.x();
		m_searchBooksByTitleLeft->changeSize(std::max(spacerNewWidth, 0), geometry.height(), QSizePolicy::Fixed, QSizePolicy::Expanding);
		const auto lineEditBookTitleToSearchNewWidth = geometry.size().width() + std::min(spacerNewWidth, 0);
		m_ui.lineEditBookTitleToSearch->setMinimumWidth(lineEditBookTitleToSearchNewWidth);
		m_ui.lineEditBookTitleToSearch->setMaximumWidth(lineEditBookTitleToSearchNewWidth);
		m_searchBooksByTitleLayout->invalidate();
	}

	void OnSearchNavigationItemSelected(long long /*id*/, const QString& text) const
	{
		m_ui.lineEditBookTitleToSearch->setText(text);
	}

	void OnStackedPageStateChanged(std::shared_ptr<QWidget> widget, const int state)
	{
		const auto reset = [this] {
			if (!m_additionalWidget)
				return;

			m_additionalWidget->setParent(nullptr);
			m_ui.stackedWidget->removeWidget(m_additionalWidget.get());
			m_additionalWidget.reset();
		};

		switch (state)
		{
			case StackedPage::State::Created:
				reset();
				m_additionalWidget = std::move(widget);
				m_ui.stackedWidget->setCurrentIndex(m_ui.stackedWidget->addWidget(m_additionalWidget.get()));
				break;

			case StackedPage::State::Finished:
			case StackedPage::State::Canceled:
				reset();
				[[fallthrough]];

			case StackedPage::State::Started:
				m_ui.stackedWidget->setCurrentIndex(0);
				break;

			default:
				assert(false && "unexpected state");
		}
	}

	void RemoveCustomStyleFile()
	{
		if (m_lastStyleFileHovered.isEmpty())
			return;

		auto list = m_settings->Get(IStyleApplier::THEME_FILES_KEY).toStringList();
		if (!erase_if(list, [this](const auto& item) {
				return item == m_lastStyleFileHovered;
			}))
			return;

		m_settings->Set(IStyleApplier::THEME_FILES_KEY, list);

		auto actions = m_ui.menuTheme->actions();
		if (const auto it = std::ranges::find(
				actions,
				m_lastStyleFileHovered,
				[](const QAction* action) {
					return action->property(IStyleApplier::ACTION_PROPERTY_THEME_FILE).toString();
				}
			);
		    it != actions.end())
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

	bool Close()
	{
		if (!m_systemTray)
			return QCoreApplication::exit(), true;

		m_isMaximized  = m_self.isMaximized();
		m_isFullScreen = m_self.isFullScreen();

		m_systemTray->show();
		m_self.hide();
		return false;
	}

	void OnStartAnotherApp() const
	{
		m_isFullScreen ? m_self.showFullScreen() : m_isMaximized ? m_self.showMaximized() : m_self.showNormal();
		if (m_systemTray)
			m_systemTray->hide();
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
			m_forwarder.Forward([&, message = QString(record.getMessage())] {
				m_ui.statusBar->showMessage(message, 2000);
			});
	}

private: // ILineOption::IObserver
	void OnOptionEditing(const QString& value) override
	{
		const auto logicFactory = ILogicFactory::Lock(m_logicFactory);
		auto       books        = logicFactory->GetExtractedBooks(m_booksWidget->GetView()->model(), m_booksWidget->GetView()->currentIndex());

		if (books.empty())
			return;

		auto scriptTemplate = value;
		auto db             = m_databaseUser->Database();

		const auto converter = logicFactory->CreateFillTemplateConverter();
		converter->Fill(*db, scriptTemplate, books.front(), Tr(MY_FOLDER));
		PLOGI << scriptTemplate;
	}

	void OnOptionEditingFinished(const QString& /*value*/) override
	{
		disconnect(m_settingsLineEditExecuteContextMenuConnection);
		QTimer::singleShot(0, [&] {
			m_lineOption->Unregister(this);
		});
	}

private: // IAlphabetPanel::IObserver
	void OnToolBarChanged() override
	{
		const auto hasVisible = [this] {
			const auto panelAlreadyAdded = m_ui.mainPageLayout->itemAt(0)->widget() == m_alphabetPanel->GetWidget();
			if (std::ranges::any_of(m_alphabetPanel->GetToolBars(), [this](const auto* toolBar) {
					return m_alphabetPanel->Visible(toolBar);
				}))
			{
				if (panelAlreadyAdded)
					return;

				m_alphabetPanel->GetWidget()->setFont(m_self.font());
				m_ui.mainPageLayout->insertWidget(0, m_alphabetPanel->GetWidget());
			}
			else if (panelAlreadyAdded)
				m_ui.mainPageLayout->removeWidget(m_alphabetPanel->GetWidget());
		};

		m_ui.menuAlphabets->clear();

		for (auto* toolBar : m_alphabetPanel->GetToolBars())
		{
			auto* action = m_ui.menuAlphabets->addAction(toolBar->accessibleName());
			action->setCheckable(true);
			action->setChecked(m_alphabetPanel->Visible(toolBar));

			connect(action, &QAction::toggled, [this, toolBar, hasVisible](const bool checked) {
				m_alphabetPanel->SetVisible(toolBar, checked);
				hasVisible();
			});
			connect(toolBar, &QToolBar::visibilityChanged, action, &QAction::setChecked);
		}

		m_ui.menuAlphabets->addSeparator();
		m_ui.menuAlphabets->addAction(m_ui.actionAddNewAlphabet);

		hasVisible();
	}

private: // ITreeViewController::::IObserver
	void OnModeChanged(const int index) override
	{
		m_ui.leftWidget->setVisible(index != static_cast<int>(NavigationMode::AllBooks));
	}

	void OnModelChanged(QAbstractItemModel* /*model*/) override
	{
	}

	void OnContextMenuTriggered(const QString& /*id*/, const IDataItem::Ptr& /*item*/) override
	{
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
		m_ui.authorAnnotationWidget->layout()->addWidget(m_authorAnnotationWidget.get());
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

		m_self.addAction(m_ui.actionShowQueryWindow);

		OnModeChanged(m_navigationViewController->GetModeIndex());
		m_navigationViewController->RegisterObserver(this);

		ReplaceMenuBar();

		QTimer::singleShot(0, [this] {
			const auto widgets = m_self.findChildren<QWidget*>();
			if (const auto it = std::ranges::find(
					widgets,
					m_settings->Get(START_FOCUSED_CONTROL, QString("SearchBooksByNames")),
					[](const QWidget* widget) {
						return widget->accessibleName();
					}
				);
			    it != widgets.cend())
				(*it)->setFocus(Qt::FocusReason::OtherFocusReason);
		});

		SetupTrayMenu();
	}

	void ReplaceMenuBar()
	{
		PLOGV << "ReplaceMenuBar";
		m_ui.lineEditBookTitleToSearch->installEventFilter(new LineEditPlaceholderTextController(m_self, *m_ui.lineEditBookTitleToSearch, Tr(SEARCH_BOOKS_BY_TITLE_PLACEHOLDER)));
		auto* menuBar              = new QWidget(&m_self);
		m_searchBooksByTitleLayout = new QHBoxLayout(menuBar);
		m_self.menuBar()->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
		m_searchBooksByTitleLayout->addWidget(m_self.menuBar());
		m_searchBooksByTitleLayout->addItem((m_searchBooksByTitleLeft = new QSpacerItem(72, 20, QSizePolicy::Fixed)));
		m_searchBooksByTitleLayout->addWidget(m_ui.lineEditBookTitleToSearch);
		m_searchBooksByTitleLayout->addItem(new QSpacerItem(72, 20, QSizePolicy::Expanding));
		m_searchBooksByTitleLayout->setContentsMargins(0, 0, 0, 0);
		m_self.setMenuWidget(menuBar);
	}

	void SetupTrayMenu()
	{
		if (!m_settings->Get(Constant::Settings::HIDE_TO_TRAY_KEY, false))
			return;

		m_systemTray = new QSystemTrayIcon(QIcon(":/icons/main.ico"), &m_self);
		auto menu    = new QMenu(&m_self);

		const auto open = [this](const auto reason = QSystemTrayIcon::Unknown) {
			if (reason != QSystemTrayIcon::ActivationReason::Context)
				OnStartAnotherApp();
		};

		menu->addAction(Tr(OPEN), open);
		menu->addAction(Tr(EXIT), [] {
			QCoreApplication::exit();
		});
		m_systemTray->setContextMenu(menu);

		connect(m_systemTray, &QSystemTrayIcon::activated, &m_self, open);
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
			if (std::ranges::any_of(ALLOW_DESTRUCTIVE_OPERATIONS_CONFIRMS, [&](const char* message) {
					return m_uiFactory->ShowWarning(Tr(message), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes;
				}))
			{
				m_ui.actionAllowDestructiveOperations->setChecked(false);
				m_uiFactory->ShowInfo(Tr(DENY_DESTRUCTIVE_OPERATIONS_MESSAGE));
				return;
			}

			m_uiFactory->ShowInfo(Tr(ALLOW_DESTRUCTIVE_OPERATIONS_MESSAGE));
		}

		m_collectionController->AllowDestructiveOperation(value);
	}

	void ShowRemovedBooks(const bool value)
	{
		m_navigationWidget->ShowRemoved(value);
		m_booksWidget->ShowRemoved(value);
	}

	void ConnectActionsFile()
	{
		PLOGV << "ConnectActionsFile";
		const auto userDataOperation = [this](const auto& operation) {
			auto controller = ILogicFactory::Lock(m_logicFactory)->CreateUserDataController();
			std::invoke(operation, *controller, [controller]() mutable {
				controller.reset();
			});
		};
		connect(m_ui.actionExportUserData, &QAction::triggered, &m_self, [=] {
			userDataOperation(&IUserDataController::Backup);
		});
		connect(m_ui.actionImportUserData, &QAction::triggered, &m_self, [=] {
			userDataOperation(&IUserDataController::Restore);
		});
		connect(m_ui.actionExit, &QAction::triggered, &m_self, [] {
			QCoreApplication::exit();
		});
	}

	void ConnectActionsCollection()
	{
		PLOGV << "ConnectActionsCollection";
		connect(m_ui.actionAddNewCollection, &QAction::triggered, &m_self, [this] {
			m_collectionController->AddCollection({});
		});
		connect(m_ui.actionRescanCollectionFolder, &QAction::triggered, &m_self, [this] {
			m_collectionController->RescanCollectionFolder();
		});
		connect(m_ui.actionRemoveCollection, &QAction::triggered, &m_self, [this] {
			m_collectionController->RemoveCollection();
		});
		connect(m_ui.actionGenerateIndexInpx, &QAction::triggered, &m_self, [this] {
			GenerateCollectionInpx();
		});
		connect(m_ui.actionShowCollectionCleaner, &QAction::triggered, &m_self, [this] {
			m_uiFactory->CreateCollectionCleaner();
		});
		ConnectSettings(m_ui.actionAllowDestructiveOperations, {}, this, &Impl::AllowDestructiveOperation);
	}

	void ConnectActionsSettingsAnnotationJokes()
	{
		static constexpr auto hasDisclaimer = "HasDisclaimer";
		static constexpr auto actionName    = "name";
		static constexpr auto visible       = "Visible";

		if (!m_settings->Get(QString(SHOW_ANNOTATION_JOKES_KEY_TEMPLATE).arg(visible), true))
		{
			delete m_ui.menuJokes;
			return;
		}

		const auto mayBeChecked = [](const QAction* item) {
			return item->isCheckable() && !item->isChecked() && !item->property(hasDisclaimer).toBool();
		};
		const auto mayBeUnchecked = [](const QAction* item) {
			return item->isCheckable() && item->isChecked();
		};

		const auto setEnabled = [this, mayBeChecked, mayBeUnchecked] {
			if (m_enableAllJokes)
				m_enableAllJokes->setEnabled(std::ranges::any_of(m_ui.menuJokes->actions(), mayBeChecked));
			if (m_disableAllJokes)
				m_disableAllJokes->setEnabled(std::ranges::any_of(m_ui.menuJokes->actions(), mayBeUnchecked));
		};

		for (const auto& [implementation, name, title, disclaimer] : m_jokeRequesterFactory->GetImplementations())
		{
			if (!m_settings->Get(QString(SHOW_ANNOTATION_JOKES_KEY_TEMPLATE).arg(name + visible), true))
				continue;

			auto* action = m_ui.menuJokes->addAction(title);
			action->setProperty(actionName, name);
			action->setProperty(hasDisclaimer, !disclaimer.isEmpty());
			action->setCheckable(true);

			connect(action, &QAction::toggled, [this, implementation, setEnabled](const bool checked) {
				m_annotationController->ShowJokes(implementation, checked);
				setEnabled();
			});
			action->setChecked(m_settings->Get(QString(SHOW_ANNOTATION_JOKES_KEY_TEMPLATE).arg(name), false));

			connect(action, &QAction::triggered, [this, action, name, disclaimer](const bool checked) {
				if (!checked || disclaimer.isEmpty() || m_uiFactory->ShowWarning(disclaimer, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
					return m_settings->Set(QString(SHOW_ANNOTATION_JOKES_KEY_TEMPLATE).arg(name), checked);
				QTimer::singleShot(0, [action] {
					action->setChecked(false);
				});
			});
		}

		m_ui.menuJokes->addSeparator();
		const auto checkAll = [this](const auto& filter, const bool checked) {
			for (auto* action : m_ui.menuJokes->actions() | std::views::filter(filter))
			{
				m_settings->Set(QString(SHOW_ANNOTATION_JOKES_KEY_TEMPLATE).arg(action->property(actionName).toString()), checked);
				action->setChecked(checked);
			}
		};
		m_enableAllJokes  = m_ui.menuJokes->addAction(Tr(ENABLE_ALL), [this, checkAll, mayBeChecked] {
            checkAll(mayBeChecked, true);
        });
		m_disableAllJokes = m_ui.menuJokes->addAction(Tr(DISABLE_ALL), [this, checkAll, mayBeUnchecked] {
			checkAll(mayBeUnchecked, false);
		});
		setEnabled();
	}

	void ConnectActionsSettingsAnnotation()
	{
		PLOGV << "ConnectActionsSettingsAnnotation";
		ConnectSettings(m_ui.actionShowAnnotationCover, SHOW_ANNOTATION_COVER_KEY, m_annotationWidget.get(), &AnnotationWidget::ShowCover);
		ConnectSettings(m_ui.actionShowAnnotationContent, SHOW_ANNOTATION_CONTENT_KEY, m_annotationWidget.get(), &AnnotationWidget::ShowContent);
		ConnectSettings(m_ui.actionShowAnnotationCoverButtons, SHOW_ANNOTATION_COVER_BUTTONS_KEY, m_annotationWidget.get(), &AnnotationWidget::ShowCoverButtons);
		ConnectSettings(m_ui.actionShowReadersReviews, SHOW_REVIEWS_KEY, m_annotationController.get(), &IAnnotationController::ShowReviews);
		connect(m_ui.actionHideAnnotation, &QAction::visibleChanged, &m_self, [&] {
			m_ui.menuAnnotation->menuAction()->setVisible(m_ui.actionHideAnnotation->isVisible());
		});

		m_ui.actionShowReadersReviews->setVisible(
			m_collectionController->ActiveCollectionExists() && QDir(m_collectionController->GetActiveCollection().GetFolder() + "/" + QString::fromStdWString(Inpx::REVIEWS_FOLDER)).exists()
		);

		ConnectActionsSettingsAnnotationJokes();
	}

	void ConnectActionsSettingsFont()
	{
		PLOGV << "ConnectActionsSettingsFont";
		const auto incrementFontSize = [&](const int value) {
			const auto fontSize = m_settings->Get(Constant::Settings::FONT_SIZE_KEY, Constant::Settings::FONT_SIZE_DEFAULT);
			m_settings->Set(Constant::Settings::FONT_SIZE_KEY, fontSize + value);
		};
		connect(m_ui.actionFontSizeUp, &QAction::triggered, &m_self, [=] {
			incrementFontSize(1);
		});
		connect(m_ui.actionFontSizeDown, &QAction::triggered, &m_self, [=] {
			incrementFontSize(-1);
		});
		connect(m_ui.actionFontSettings, &QAction::triggered, &m_self, [&] {
			if (const auto font = m_uiFactory->GetFont(Tr(FONT_DIALOG_TITLE), m_self.font()))
			{
				const SettingsGroup group(*m_settings, Constant::Settings::FONT_KEY);
				Util::Serialize(*font, *m_settings);
			}
		});
	}

	void ConnectActionsSettingsAlphabet()
	{
		connect(m_ui.actionAddNewAlphabet, &QAction::triggered, [this] {
			m_alphabetPanel->AddNewAlphabet();
		});
		OnToolBarChanged();
		m_alphabetPanel->RegisterObserver(this);
	}

	void ConnectActionsSettingsLog()
	{
		PLOGV << "ConnectActionsSettingsLog";
		connect(m_ui.logView, &QWidget::customContextMenuRequested, &m_self, [&](const QPoint& pos) {
			m_ui.menuLog->exec(m_ui.logView->viewport()->mapToGlobal(pos));
		});
		connect(m_ui.actionShowLog, &QAction::triggered, &m_self, [&](const bool checked) {
			m_ui.stackedWidget->setCurrentIndex(checked ? 1 : m_ui.stackedWidget->count() > 2 ? m_ui.stackedWidget->count() - 1 : 0);
		});
		connect(m_ui.stackedWidget, &QStackedWidget::currentChanged, &m_self, [this](const int currentIndex) {
			m_ui.lineEditBookTitleToSearch->setVisible(m_ui.actionShowSearchBookString->isChecked() && currentIndex == 0);
		});

		connect(m_ui.actionClearLog, &QAction::triggered, &m_self, [&] {
			m_logController->Clear();
		});
		const auto logAction = [this](const auto& action) {
			if (!m_ui.actionShowLog->isChecked())
				m_ui.actionShowLog->trigger();
			std::invoke(action, *m_logController);
		};
		connect(m_ui.actionShowCollectionStatistics, &QAction::triggered, &m_self, [=] {
			logAction(&ILogController::ShowCollectionStatistics);
		});
		connect(m_ui.actionTestLogColors, &QAction::triggered, &m_self, [=] {
			logAction(&ILogController::TestColors);
		});
	}

	void ConnectActionsSettingsTheme()
	{
		PLOGV << "ConnectActionsSettingsTheme";
		connect(m_ui.actionAddThemes, &QAction::triggered, &m_self, [this] {
			OpenExternalStyle(m_uiFactory->GetOpenFileNames(QSS, Tr(SELECT_QSS_FILE), Tr(QSS_FILE_FILTER).arg(QSS)));
		});
		connect(m_ui.actionDeleteAllThemes, &QAction::triggered, &m_self, [this] {
			DeleteCustomThemes();
		});
	}

	void ConnectActionsSettingsExport()
	{
		PLOGV << "ConnectActionsSettingsExport";
		connect(m_ui.actionExportTemplate, &QAction::triggered, &m_self, [&] {
			m_settingsLineEditExecuteContextMenuConnection = connect(m_ui.settingsLineEdit, &QWidget::customContextMenuRequested, &m_self, [this] {
				{
					QSignalBlocker blocker(m_ui.settingsLineEdit);
					IScriptController::ExecuteContextMenu(m_ui.settingsLineEdit);
				}
				emit m_ui.settingsLineEdit->textChanged(m_ui.settingsLineEdit->text());
			});
			m_lineOption->Register(this);
			m_lineOption->SetSettingsKey(Constant::Settings::EXPORT_TEMPLATE_KEY, IScriptController::GetDefaultOutputFileNameTemplate());
		});

		m_ui.menuImages->setEnabled(
			m_collectionController->ActiveCollectionExists() && QDir(m_collectionController->GetActiveCollection().GetFolder() + "/" + Global::COVERS).exists()
			&& QDir(m_collectionController->GetActiveCollection().GetFolder() + "/" + Global::IMAGES).exists()
		);

		ConnectSettings(m_ui.actionExportRewriteMetadata, Constant::Settings::EXPORT_REPLACE_METADATA_KEY);
		ConnectSettings(m_ui.actionExportConvertCoverToGrayscale, Constant::Settings::EXPORT_GRAYSCALE_COVER_KEY);
		ConnectSettings(m_ui.actionExportConvertImagesToGrayscale, Constant::Settings::EXPORT_GRAYSCALE_IMAGES_KEY);
		ConnectSettings(m_ui.actionExportRemoveCover, Constant::Settings::EXPORT_REMOVE_COVER_KEY);
		ConnectSettings(m_ui.actionExportRemoveImages, Constant::Settings::EXPORT_REMOVE_IMAGES_KEY);
	}

	void ConnectActionsSettingsView()
	{
		PLOGV << "ConnectActionsSettingsView";
		ConnectSettings(m_ui.actionShowRemoved, Constant::Settings::SHOW_REMOVED_BOOKS_KEY, this, &Impl::ShowRemovedBooks);
		ConnectSettings(m_ui.actionShowStatusBar, SHOW_STATUS_BAR_KEY, qobject_cast<QWidget*>(m_ui.statusBar), &QWidget::setVisible);
		ConnectSettings(m_ui.actionShowSearchBookString, SHOW_SEARCH_BOOK_KEY, qobject_cast<QWidget*>(m_ui.lineEditBookTitleToSearch), &QWidget::setVisible);
		ConnectSettings(m_ui.actionShowAuthorAnnotation, SHOW_AUTHOR_ANNOTATION_KEY, m_authorAnnotationWidget.get(), &AuthorAnnotationWidget::Show);
		ConnectShowHide(m_ui.annotationWidget, &QWidget::setVisible, m_ui.actionShowAnnotation, m_ui.actionHideAnnotation, SHOW_ANNOTATION_KEY);

		m_ui.actionShowAuthorAnnotation->setVisible(
			m_collectionController->ActiveCollectionExists() && QDir(m_collectionController->GetActiveCollection().GetFolder() + "/" + QString::fromStdWString(Inpx::AUTHORS_FOLDER)).exists()
		);

		auto restoreDefaultSettings = [this] {
			if (m_uiFactory->ShowQuestion(Tr(CONFIRM_RESTORE_DEFAULT_SETTINGS), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
				return;

			m_settings->Remove("ui");
			Reboot();
		};
		connect(m_ui.actionRestoreDefaultSettings, &QAction::triggered, &m_self, [restoreDefaultSettings = std::move(restoreDefaultSettings)] {
			restoreDefaultSettings();
		});

		ConnectActionsSettingsAnnotation();
		ConnectActionsSettingsFont();
		ConnectActionsSettingsAlphabet();
		ConnectActionsSettingsLog();
		ConnectActionsSettingsTheme();
	}

	void ConnectActionsSettingsHttp()
	{
		connect(m_ui.actionHttpServerManagement, &QAction::triggered, &m_self, [&] {
			try
			{
				m_uiFactory->CreateOpdsDialog()->exec();
			}
			catch (const std::exception& ex)
			{
				m_uiFactory->ShowError(QString::fromStdString(ex.what()));
			}
		});
		const auto browse = [this](const QString& folder = {}) {
			const auto url = QString("http://localhost:%1/%2").arg(m_settings->Get(Constant::Settings::OPDS_PORT_KEY, Constant::Settings::OPDS_PORT_DEFAULT)).arg(folder);
			if (!QDesktopServices::openUrl(url))
				PLOGE << "Cannot open " << url;
		};
		connect(m_ui.actionBrowseHttpOpds, &QAction::triggered, [=] {
			browse("opds");
		});
		connect(m_ui.actionBrowseHttpSite, &QAction::triggered, [=] {
			browse();
		});
		connect(m_ui.actionBrowseHttpWeb, &QAction::triggered, [=] {
			browse("web");
		});
		connect(m_ui.menuHttp, &QMenu::aboutToShow, [this] {
			auto       controller = ILogicFactory::Lock(m_logicFactory)->CreateOpdsController();
			const auto running    = controller->IsRunning();
			m_ui.actionBrowseHttpOpds->setEnabled(running);
			m_ui.actionBrowseHttpSite->setEnabled(running);
			m_ui.actionBrowseHttpWeb->setEnabled(running);
		});
	}

	void ConnectActionsSettings()
	{
		PLOGV << "ConnectActionsSettings";
		ConnectActionsSettingsExport();
		ConnectActionsSettingsView();
		ConnectActionsSettingsHttp();

		connect(m_localeController.get(), &LocaleController::LocaleChanged, &m_self, [&] {
			Reboot();
		});
		connect(m_ui.actionFilters, &QAction::triggered, &m_self, [&] {
			m_uiFactory->CreateFilterSettingsDialog()->exec();
		});
		connect(m_ui.actionScripts, &QAction::triggered, &m_self, [&] {
			m_uiFactory->CreateScriptDialog()->Exec();
		});
	}

	void ConnectActionsHelp()
	{
		PLOGV << "ConnectActionsHelp";
		connect(m_ui.actionCheckForUpdates, &QAction::triggered, &m_self, [&] {
			CheckForUpdates(true);
		});
		connect(m_ui.actionAbout, &QAction::triggered, &m_self, [&] {
			m_uiFactory->ShowAbout();
		});
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
		connect(m_ui.lineEditBookTitleToSearch, &QLineEdit::returnPressed, &m_self, [this] {
			SearchBookByTitle();
		});
		connect(m_ui.actionSearchBookByTitle, &QAction::triggered, &m_self, [this] {
			SearchBookByTitle();
		});
		connect(m_ui.actionShowQueryWindow, &QAction::triggered, &m_self, [this] {
			m_queryWindow = m_uiFactory->CreateQueryWindow();
			m_queryWindow->show();
		});
	}

	template <typename T = QAction>
	void ConnectSettings(QAction* action, QString key, T* obj = nullptr, void (T::*f)(bool) = nullptr)
	{
		if (!key.isEmpty())
			action->setChecked(m_settings->Get(key, action->isChecked()));
		auto invoker = [obj, f](const bool checked) {
			if (obj && f)
				std::invoke(f, obj, checked);
		};
		invoker(action->isChecked());

		connect(action, &QAction::triggered, &m_self, [this, invoker = std::move(invoker), key = std::move(key)](const bool checked) {
			if (!key.isEmpty())
				m_settings->Set(key, checked);
			invoker(checked);
		});
	}

	template <typename T>
	void ConnectShowHide(T* obj, void (T::*f)(bool), QAction* show, QAction* hide, const char* key = nullptr, const bool defaultValue = true)
	{
		auto apply = [=](const bool value) {
			std::invoke(f, obj, value);
			hide->setVisible(value);
			show->setVisible(!value);
		};

		if (key)
		{
			const auto value = m_settings->Get(key, defaultValue);
			apply(value);
		}

		const auto showHide = [&settings = *m_settings, key, apply = std::move(apply)](const bool value) {
			if (key)
				settings.Set(key, value);
			apply(value);
		};

		connect(show, &QAction::triggered, &m_self, [=] {
			showHide(true);
		});
		connect(hide, &QAction::triggered, &m_self, [=] {
			showHide(false);
		});
	}

	void ApplyStyleAction(QAction& action, const QActionGroup& group) const
	{
		PLOGV << "ApplyStyleAction";
		for (auto* groupAction : group.actions())
			groupAction->setEnabled(true);

		action.setEnabled(false);

		auto applier = m_styleApplierFactory->CreateStyleApplier(static_cast<IStyleApplier::Type>(action.property(IStyleApplier::ACTION_PROPERTY_THEME_TYPE).toInt()));
		applier->Apply(action.property(IStyleApplier::ACTION_PROPERTY_THEME_NAME).toString(), action.property(IStyleApplier::ACTION_PROPERTY_THEME_FILE).toString());
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
			connect(action, &QAction::triggered, &m_self, [this, action, group] {
				ApplyStyleAction(*action, *group);
			});
		}
	}

	QAction* CreateStyleAction(QMenu& menu, const IStyleApplier::Type type, const QString& actionName, const QString& name, const QString& file = {})
	{
		auto* action = menu.addAction(QFileInfo(actionName).completeBaseName());

		action->setProperty(IStyleApplier::ACTION_PROPERTY_THEME_NAME, name);
		action->setProperty(IStyleApplier::ACTION_PROPERTY_THEME_TYPE, static_cast<int>(type));
		action->setProperty(IStyleApplier::ACTION_PROPERTY_THEME_FILE, file);
		action->setCheckable(true);

		connect(action, &QAction::hovered, &m_self, [this, file] {
			m_lastStyleFileHovered = file;
		});

		return action;
	}

	void CreateStylesMenu()
	{
		PLOGV << "CreateStylesMenu";
		const auto addActionGroup = [this](const std::vector<QAction*>& actions, QActionGroup* group) {
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
			return AddExternalStyleDll(fileInfo);

		return {};
	}

	std::vector<QAction*> AddExternalStyleDll(const QFileInfo& fileInfo)
	{
		const auto addLibList = [&](const std::set<QString>& libList) -> std::vector<QAction*> {
			if (libList.empty())
				return {};

			auto* menu = m_ui.menuTheme->addMenu(fileInfo.completeBaseName());
			menu->setFont(m_self.font());

			auto* action = menu->menuAction();
			action->setProperty(IStyleApplier::ACTION_PROPERTY_THEME_FILE, fileInfo.filePath());
			connect(action, &QAction::hovered, &m_self, [this, file = fileInfo.filePath()] {
				m_lastStyleFileHovered = file;
			});

			std::vector<QAction*> result;
			result.reserve(libList.size());
			std::ranges::transform(libList, std::back_inserter(result), [&](const auto& qss) {
				return CreateStyleAction(*menu, IStyleApplier::Type::DllStyle, qss, qss, fileInfo.filePath());
			});

			return result;
		};

		auto currentList = GetQssList();

		if (auto applier = m_styleApplierFactory->CreateThemeApplier(); applier->GetType() == IStyleApplier::Type::DllStyle && applier->GetChecked().second == fileInfo.filePath())
		{
			currentList.erase(IStyleApplier::STYLE_FILE_NAME);
			return addLibList(currentList);
		}

		Util::DyLib lib(fileInfo.filePath().toStdString());
		if (!lib.IsOpen())
		{
			PLOGE << lib.GetErrorDescription();
			return {};
		}

		auto libList = GetQssList();
		erase_if(libList, [&](const auto& item) {
			return currentList.contains(item);
		});
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
			const auto file = action->property(IStyleApplier::ACTION_PROPERTY_THEME_FILE).toString();
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

		auto searchString = m_ui.lineEditBookTitleToSearch->text().toLower();
		searchString.removeIf([](const QChar ch) {
			return ch != ' ' && ch.category() != QChar::Letter_Lowercase;
		});

		if (searchString.isEmpty())
			return;

		auto  searchController    = ILogicFactory::Lock(m_logicFactory)->CreateSearchController();
		auto& searchControllerRef = *searchController;
		searchControllerRef.Search(searchString, [this, searchController = std::move(searchController)](const long long id) mutable {
			if (id <= 0)
				return;

			m_navigationWidget->SetMode(static_cast<int>(NavigationMode::Search), QString::number(id));
			searchController.reset();
		});
	}

	void CreateLogMenu()
	{
		PLOGV << "CreateLogMenu";
		auto*      group           = new QActionGroup(&m_self);
		const auto currentSeverity = m_logController->GetSeverity();
		std::ranges::for_each(m_logController->GetSeverities(), [&, n = 0](const char* name) mutable {
			auto* action = m_ui.menuLogVerbosityLevel->addAction(Loc::Tr(Loc::Ctx::LOGGING, name), [&, n] {
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
			auto*      action = m_ui.menuSelectCollection->addAction(collection->name);
			connect(action, &QAction::triggered, &m_self, [&, id = collection->id] {
				m_collectionController->SetActiveCollection(id);
			});
			action->setCheckable(true);
			action->setChecked(active);
			action->setEnabled(!active);
			group->addAction(action);
		}

		const auto enabled = !m_ui.menuSelectCollection->isEmpty();
		m_ui.actionRemoveCollection->setEnabled(enabled);
		m_ui.menuSelectCollection->setEnabled(enabled);

		if (m_ui.menuSelectCollection->actions().count() < 2)
			m_ui.menuCollection->removeAction(m_ui.menuSelectCollection->menuAction());
	}

	void CreateViewNavigationMenu()
	{
		const auto createAction = [this](const char* name) {
			auto* action = m_ui.menuNavigation->addAction(Loc::Tr(Loc::NAVIGATION, name));
			action->setCheckable(true);
			action->setChecked(true);
			ConnectSettings(action, QString(Constant::Settings::VIEW_NAVIGATION_KEY_TEMPLATE).arg(name));
			connect(action, &QAction::toggled, [this] {
				RebootDialog();
			});
		};

#define NAVIGATION_MODE_ITEM(NAME) createAction(#NAME);
		NAVIGATION_MODE_ITEMS_X_MACRO
#undef NAVIGATION_MODE_ITEM
	}

	void CheckForUpdates(const bool force) const
	{
		auto updateChecker = ILogicFactory::Lock(m_logicFactory)->CreateUpdateChecker();
		updateChecker->CheckForUpdate(force, [updateChecker]() mutable {
			updateChecker.reset();
		});
	}

	void StartDelayed(std::function<void()> f)
	{
		PLOGV << "StartDelayed";
		connect(&m_delayStarter, &QTimer::timeout, &m_self, [this, f = std::move(f)] {
			m_delayStarter.disconnect();
			f();
		});
		m_delayStarter.start();
	}

	void GenerateCollectionInpx() const
	{
		auto  inpxGenerator    = ILogicFactory::Lock(m_logicFactory)->CreateInpxGenerator();
		auto& inpxGeneratorRef = *inpxGenerator;
		if (auto inpxFileName = m_uiFactory->GetSaveFileName(Constant::Settings::EXPORT_DIALOG_KEY, Loc::Tr(Loc::EXPORT, Loc::SELECT_INPX_FILE), Loc::Tr(Loc::EXPORT, Loc::SELECT_INPX_FILE_FILTER));
		    !inpxFileName.isEmpty())
			inpxGeneratorRef.GenerateInpx(std::move(inpxFileName), [inpxGenerator = std::move(inpxGenerator)](bool) mutable {
				inpxGenerator.reset();
			});
	}

	void SetCheckForUpdateOnStartEnabled(const bool value) noexcept
	{
		m_checkForUpdateOnStartEnabled = value;
	}

	static void Reboot()
	{
		QTimer::singleShot(0, [] {
			QCoreApplication::exit(Constant::RESTART_APP);
		});
	}

private:
	MainWindow&    m_self;
	Ui::MainWindow m_ui {};
	QTimer         m_delayStarter;

	std::weak_ptr<const ILogicFactory>           m_logicFactory;
	std::shared_ptr<const IStyleApplierFactory>  m_styleApplierFactory;
	std::shared_ptr<const IJokeRequesterFactory> m_jokeRequesterFactory;
	std::shared_ptr<const IUiFactory>            m_uiFactory;
	std::shared_ptr<const IDatabaseUser>         m_databaseUser;

	PropagateConstPtr<ISettings, std::shared_ptr>              m_settings;
	PropagateConstPtr<ICollectionController, std::shared_ptr>  m_collectionController;
	PropagateConstPtr<IParentWidgetProvider, std::shared_ptr>  m_parentWidgetProvider;
	PropagateConstPtr<IAnnotationController, std::shared_ptr>  m_annotationController;
	PropagateConstPtr<AnnotationWidget, std::shared_ptr>       m_annotationWidget;
	PropagateConstPtr<AuthorAnnotationWidget, std::shared_ptr> m_authorAnnotationWidget;
	PropagateConstPtr<LocaleController, std::shared_ptr>       m_localeController;
	PropagateConstPtr<ILogController, std::shared_ptr>         m_logController;
	PropagateConstPtr<QWidget, std::shared_ptr>                m_progressBar;
	PropagateConstPtr<QStyledItemDelegate, std::shared_ptr>    m_logItemDelegate;
	PropagateConstPtr<ILineOption, std::shared_ptr>            m_lineOption;
	PropagateConstPtr<IAlphabetPanel, std::shared_ptr>         m_alphabetPanel;

	PropagateConstPtr<ITreeViewController, std::shared_ptr> m_navigationViewController;

	PropagateConstPtr<TreeView, std::shared_ptr> m_booksWidget;
	PropagateConstPtr<TreeView, std::shared_ptr> m_navigationWidget;

	std::shared_ptr<QMainWindow> m_queryWindow;
	std::shared_ptr<QWidget>     m_additionalWidget;

	Util::FunctorExecutionForwarder m_forwarder;
	const Log::LogAppender          m_logAppender { this };

	QMetaObject::Connection m_settingsLineEditExecuteContextMenuConnection;
	QSpacerItem*            m_searchBooksByTitleLeft;
	QLayout*                m_searchBooksByTitleLayout;

	bool m_checkForUpdateOnStartEnabled { true };

	QActionGroup* m_stylesActionGroup { new QActionGroup(&m_self) };
	QString       m_lastStyleFileHovered;

	QAction* m_enableAllJokes { nullptr };
	QAction* m_disableAllJokes { nullptr };

	QSystemTrayIcon* m_systemTray { nullptr };
	bool             m_isMaximized { false };
	bool             m_isFullScreen { false };
};

MainWindow::MainWindow(
	const std::shared_ptr<const ILogicFactory>&     logicFactory,
	std::shared_ptr<const IStyleApplierFactory>     styleApplierFactory,
	std::shared_ptr<const IJokeRequesterFactory>    jokeRequesterFactory,
	std::shared_ptr<const IUiFactory>               uiFactory,
	std::shared_ptr<const IDatabaseUser>            databaseUser,
	std::shared_ptr<const ICollectionUpdateChecker> collectionUpdateChecker,
	std::shared_ptr<const IDatabaseChecker>         databaseChecker,
	std::shared_ptr<const ICommandLine>             commandLine,
	std::shared_ptr<ISettings>                      settings,
	std::shared_ptr<ICollectionController>          collectionController,
	std::shared_ptr<IParentWidgetProvider>          parentWidgetProvider,
	std::shared_ptr<IAnnotationController>          annotationController,
	std::shared_ptr<AnnotationWidget>               annotationWidget,
	std::shared_ptr<AuthorAnnotationWidget>         authorAnnotationWidget,
	std::shared_ptr<LocaleController>               localeController,
	std::shared_ptr<ILogController>                 logController,
	std::shared_ptr<ProgressBar>                    progressBar,
	std::shared_ptr<LogItemDelegate>                logItemDelegate,
	std::shared_ptr<ILineOption>                    lineOption,
	std::shared_ptr<IAlphabetPanel>                 alphabetPanel,
	QWidget*                                        parent
)
	: QMainWindow(parent)
	, m_impl(
		  *this,
		  logicFactory,
		  std::move(styleApplierFactory),
		  std::move(jokeRequesterFactory),
		  std::move(uiFactory),
		  std::move(databaseUser),
		  std::move(collectionUpdateChecker),
		  std::move(databaseChecker),
		  std::move(commandLine),
		  std::move(settings),
		  std::move(collectionController),
		  std::move(parentWidgetProvider),
		  std::move(annotationController),
		  std::move(annotationWidget),
		  std::move(authorAnnotationWidget),
		  std::move(localeController),
		  std::move(logController),
		  std::move(progressBar),
		  std::move(logItemDelegate),
		  std::move(lineOption),
		  std::move(alphabetPanel)
	  )
{
	Util::ObjectsConnector::registerEmitter(ObjectConnectorID::BOOK_TITLE_TO_SEARCH_VISIBLE_CHANGED, this, SIGNAL(BookTitleToSearchVisibleChanged()));
	Util::ObjectsConnector::registerReceiver(ObjectConnectorID::BOOKS_SEARCH_FILTER_VALUE_GEOMETRY_CHANGED, this, SLOT(OnBooksSearchFilterValueGeometryChanged(const QRect&)), true);
	Util::ObjectsConnector::registerReceiver(ObjectConnectorID::SEARCH_NAVIGATION_ITEM_SELECTED, this, SLOT(OnSearchNavigationItemSelected(long long, const QString&)), true);
	Util::ObjectsConnector::registerReceiver(ObjectConnectorID::STACKED_PAGE_STATE_CHANGED, this, SLOT(OnStackedPageStateChanged(std::shared_ptr<QWidget>, int)), true);
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

void MainWindow::OnStartAnotherApp()
{
	m_impl->OnStartAnotherApp();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	if (!m_impl->Close())
		event->ignore();
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

void MainWindow::OnSearchNavigationItemSelected(const long long id, const QString& text)
{
	m_impl->OnSearchNavigationItemSelected(id, text);
}

void MainWindow::OnStackedPageStateChanged(std::shared_ptr<QWidget> widget, const int state)
{
	m_impl->OnStackedPageStateChanged(std::move(widget), state);
}
