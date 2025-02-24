#include "ui_MainWindow.h"

#include "MainWindow.h"

#include <QActionEvent>
#include <QActionGroup>
#include <QDirIterator>
#include <QGuiApplication>
#include <QPainter>
#include <QStyleFactory>
#include <QStyleHints>
#include <QTimer>

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"
#include "interface/constants/ObjectConnectionID.h"
#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/IBookSearchController.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/ICollectionUpdateChecker.h"
#include "interface/logic/ICommandLine.h"
#include "interface/logic/IDatabaseChecker.h"
#include "interface/logic/IInpxGenerator.h"
#include "interface/logic/ILogController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IScriptController.h"
#include "interface/logic/ITreeViewController.h"
#include "interface/logic/IUpdateChecker.h"
#include "interface/logic/IUserDataController.h"
#include "interface/ui/ILineOption.h"
#include "interface/ui/IUiFactory.h"
#include "interface/ui/dialogs/IScriptDialog.h"

#include "GuiUtil/GeometryRestorable.h"
#include "GuiUtil/interface/IParentWidgetProvider.h"
#include "GuiUtil/util.h"
#include "logging/LogAppender.h"
#include "util/DyLib.h"
#include "util/FunctorExecutionForwarder.h"
#include "util/ISettings.h"
#include "util/ObjectsConnector.h"
#include "util/serializer/Font.h"

#include "AnnotationWidget.h"
#include "LocaleController.h"
#include "LogItemDelegate.h"
#include "ProgressBar.h"
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
constexpr auto DATABASE_BROKEN = QT_TRANSLATE_NOOP("MainWindow", "Database file \"%1\" is probably corrupted");
constexpr auto DENY_DESTRUCTIVE_OPERATIONS_MESSAGE = QT_TRANSLATE_NOOP("MainWindow", "The right decision!");
constexpr auto ALLOW_DESTRUCTIVE_OPERATIONS_MESSAGE = QT_TRANSLATE_NOOP("MainWindow", "Well, you only have yourself to blame!");
constexpr auto SELECT_QSS_FILE = QT_TRANSLATE_NOOP("MainWindow", "Select stylesheet file");
constexpr auto QSS_FILE_FILTER = QT_TRANSLATE_NOOP("MainWindow", "Qt stylesheet files (*.%1 *.dll);;All files (*.*)");
constexpr auto SEARCH_BOOKS_BY_TITLE_PLACEHOLDER = QT_TRANSLATE_NOOP("MainWindow", "To search books by title, enter part of the title here and press enter");
constexpr auto EXTERNAL_STYLESHEET = QT_TRANSLATE_NOOP("MainWindow", "Select external stylesheet");
constexpr auto EXTERNAL_STYLESHEET_FILE_NAME = QT_TRANSLATE_NOOP("MainWindow", "Stylesheet file name (*.qss)");
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
constexpr auto ACTION_PROPERTY_NAME = "value";
constexpr auto QSS = "qss";

class AllowDestructiveOperationsObserver final : public QObject
{
public:
	AllowDestructiveOperationsObserver(std::function<void()> function, std::shared_ptr<const IUiFactory> uiFactory, QObject* parent = nullptr)
		: QObject(parent)
		, m_function { std::move(function) }
		, m_uiFactory { std::move(uiFactory) }
	{
	}

private: // QObject
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		if (event->type() != QEvent::ActionChanged)
			return QObject::eventFilter(watched, event);

		const auto* actionChanged = static_cast<QActionEvent*>(event); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
		if (actionChanged->action()->isEnabled())
			m_uiFactory->ShowInfo(Tr(DENY_DESTRUCTIVE_OPERATIONS_MESSAGE));
		else if (std::ranges::any_of(ALLOW_DESTRUCTIVE_OPERATIONS_CONFIRMS,
		                             [&](const char* message) { return m_uiFactory->ShowWarning(Tr(message), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes; }))
			m_function();
		else
			m_uiFactory->ShowInfo(Tr(ALLOW_DESTRUCTIVE_OPERATIONS_MESSAGE));

		return QObject::eventFilter(watched, event);
	}

private:
	const std::function<void()> m_function;
	const std::shared_ptr<const IUiFactory> m_uiFactory;
};

class LineEditPlaceholderTextController final : public QObject
{
public:
	LineEditPlaceholderTextController(MainWindow& mainWindow, QLineEdit& lineEdit, const char* placeholderText)
		: QObject(&lineEdit)
		, m_mainWindow { mainWindow }
		, m_lineEdit { lineEdit }
		, m_placeholderText { placeholderText }
	{
	}

private: // QObject
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		if (event->type() == QEvent::Enter)
			m_lineEdit.setPlaceholderText(Tr(m_placeholderText));
		else if (event->type() == QEvent::Leave)
			m_lineEdit.setPlaceholderText({});
		else if (event->type() == QEvent::Show)
			emit m_mainWindow.BookTitleToSearchVisibleChanged();
		return QObject::eventFilter(watched, event);
	}

private:
	MainWindow& m_mainWindow;
	QLineEdit& m_lineEdit;
	const char* const m_placeholderText;
};

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
		, m_self(self)
		, m_logicFactory(logicFactory)
		, m_uiFactory(std::move(uiFactory))
		, m_settings(std::move(settings))
		, m_collectionController(std::move(collectionController))
		, m_parentWidgetProvider(std::move(parentWidgetProvider))
		, m_annotationController { std::move(annotationController) }
		, m_annotationWidget(std::move(annotationWidget))
		, m_localeController(std::move(localeController))
		, m_logController(std::move(logController))
		, m_progressBar(std::move(progressBar))
		, m_logItemDelegate(std::move(logItemDelegate))
		, m_lineOption(std::move(lineOption))
		, m_booksWidget(m_uiFactory->CreateTreeViewWidget(ItemType::Books))
		, m_navigationWidget(m_uiFactory->CreateTreeViewWidget(ItemType::Navigation))
	{
		Setup();
		ConnectActions();
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
		m_ui.setupUi(&m_self);

		m_self.setWindowTitle(PRODUCT_ID);

		m_parentWidgetProvider->SetWidget(&m_self);

		m_delayStarter.setSingleShot(true);
		m_delayStarter.setInterval(std::chrono::milliseconds(200));

		m_ui.navigationWidget->layout()->addWidget(m_navigationWidget.get());
		m_ui.annotationWidget->layout()->addWidget(m_annotationWidget.get());
		m_ui.booksWidget->layout()->addWidget(m_booksWidget.get());
		m_ui.booksWidget->layout()->addWidget(m_progressBar.get());

		m_localeController->Setup(*m_ui.menuLanguage);

		m_ui.logView->setModel(m_logController->GetModel());
		m_ui.logView->setItemDelegate(m_logItemDelegate.get());

		m_ui.settingsLineEdit->setVisible(false);
		m_lineOption->SetLineEdit(m_ui.settingsLineEdit);

		m_ui.lineEditBookTitleToSearch->addAction(m_ui.actionSearchBookByTitle, QLineEdit::LeadingPosition);

		OnObjectVisibleChanged(m_booksWidget.get(), &TreeView::ShowRemoved, m_ui.actionShowRemoved, m_ui.actionHideRemoved, m_settings->Get(SHOW_REMOVED_BOOKS_KEY, true));
		OnObjectVisibleChanged(m_ui.annotationWidget, &QWidget::setVisible, m_ui.actionShowAnnotation, m_ui.menuAnnotation->menuAction(), m_settings->Get(SHOW_ANNOTATION_KEY, true));
		OnObjectVisibleChanged(m_annotationWidget.get(),
		                       &AnnotationWidget::ShowContent,
		                       m_ui.actionShowAnnotationContent,
		                       m_ui.actionHideAnnotationContent,
		                       m_settings->Get(SHOW_ANNOTATION_CONTENT_KEY, true));
		OnObjectVisibleChanged(m_annotationWidget.get(), &AnnotationWidget::ShowCover, m_ui.actionShowAnnotationCover, m_ui.actionHideAnnotationCover, m_settings->Get(SHOW_ANNOTATION_COVER_KEY, true));
		OnObjectVisibleChanged(m_annotationWidget.get(),
		                       &AnnotationWidget::ShowCoverButtons,
		                       m_ui.actionShowAnnotationCoverButtons,
		                       m_ui.actionHideAnnotationCoverButtons,
		                       m_settings->Get(SHOW_ANNOTATION_COVER_BUTTONS_KEY, true));
		OnObjectVisibleChanged<QStatusBar>(m_ui.statusBar, &QWidget::setVisible, m_ui.actionShowStatusBar, m_ui.actionHideStatusBar, m_settings->Get(SHOW_STATUS_BAR_KEY, true));
		OnObjectVisibleChanged<QLineEdit>(m_ui.lineEditBookTitleToSearch, &QWidget::setVisible, m_ui.actionShowSearchBookString, m_ui.actionHideSearchBookString, m_settings->Get(SHOW_SEARCH_BOOK_KEY, true));
		if (m_collectionController->ActiveCollectionExists())
		{
			OnObjectVisibleChanged(this,
			                       &Impl::AllowDestructiveOperation,
			                       m_ui.actionAllowDestructiveOperations,
			                       m_ui.actionDenyDestructiveOperations,
			                       m_collectionController->GetActiveCollection().destructiveOperationsAllowed);
			m_ui.actionAllowDestructiveOperations->installEventFilter(new AllowDestructiveOperationsObserver(
				[this] { OnObjectVisibleChanged(this, &Impl::AllowDestructiveOperation, m_ui.actionAllowDestructiveOperations, m_ui.actionDenyDestructiveOperations, false); },
				m_uiFactory,
				&m_self));
		}
		else
		{
			m_ui.actionAllowDestructiveOperations->setVisible(false);
			m_ui.actionDenyDestructiveOperations->setVisible(false);
		}
		m_ui.actionPermanentLanguageFilter->setChecked(m_settings->Get(Constant::Settings::KEEP_RECENT_LANG_FILTER_KEY, false));
		m_ui.actionShowJokes->setChecked(m_settings->Get(SHOW_JOKES_KEY, false));
		m_annotationController->ShowJokes(m_ui.actionShowJokes->isChecked());

		if (const auto severity = m_settings->Get(LOG_SEVERITY_KEY); severity.isValid())
			m_logController->SetSeverity(severity.toInt());

		if (m_collectionController->ActiveCollectionExists())
			m_self.setWindowTitle(QString("%1 - %2").arg(PRODUCT_ID).arg(m_collectionController->GetActiveCollection().name));

		ReplaceMenuBar();
	}

	void ReplaceMenuBar()
	{
		m_ui.lineEditBookTitleToSearch->installEventFilter(new LineEditPlaceholderTextController(m_self, *m_ui.lineEditBookTitleToSearch, SEARCH_BOOKS_BY_TITLE_PLACEHOLDER));
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
		m_collectionController->AllowDestructiveOperation(value);
		m_ui.actionShowCollectionCleaner->setEnabled(value);
	}

	void ConnectActions()
	{
		const auto incrementFontSize = [&](const int value)
		{
			const auto fontSize = m_settings->Get(Constant::Settings::FONT_SIZE_KEY, Constant::Settings::FONT_SIZE_DEFAULT);
			m_settings->Set(Constant::Settings::FONT_SIZE_KEY, fontSize + value);
		};
		connect(m_ui.actionFontSizeUp, &QAction::triggered, &m_self, [=] { incrementFontSize(1); });
		connect(m_ui.actionFontSizeDown, &QAction::triggered, &m_self, [=] { incrementFontSize(-1); });

		connect(m_ui.actionExit, &QAction::triggered, &m_self, [] { QCoreApplication::exit(); });
		connect(m_ui.actionAddNewCollection, &QAction::triggered, &m_self, [&] { m_collectionController->AddCollection({}); });
		connect(m_ui.actionCheckForUpdates, &QAction::triggered, &m_self, [&] { CheckForUpdates(true); });
		connect(m_ui.actionAbout, &QAction::triggered, &m_self, [&] { m_uiFactory->ShowAbout(); });
		connect(m_localeController.get(), &LocaleController::LocaleChanged, &m_self, [&] { Reboot(); });
		connect(m_ui.actionRemoveCollection, &QAction::triggered, &m_self, [&] { m_collectionController->RemoveCollection(); });
		connect(m_ui.logView, &QWidget::customContextMenuRequested, &m_self, [&](const QPoint& pos) { m_ui.menuLog->exec(m_ui.logView->viewport()->mapToGlobal(pos)); });
		connect(m_ui.actionShowLog, &QAction::triggered, &m_self, [&](const bool checked) { m_ui.stackedWidget->setCurrentIndex(checked ? 1 : 0); });
		connect(m_ui.actionClearLog, &QAction::triggered, &m_self, [&] { m_logController->Clear(); });
		connect(m_ui.actionShowCollectionStatistics,
		        &QAction::triggered,
		        &m_self,
		        [&]
		        {
					m_logController->ShowCollectionStatistics();
					if (!m_ui.actionShowLog->isChecked())
						m_ui.actionShowLog->trigger();
				});
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
		connect(m_ui.actionRestoreDefaultSettings,
		        &QAction::triggered,
		        &m_self,
		        [&]
		        {
					if (m_uiFactory->ShowQuestion(Tr(CONFIRM_RESTORE_DEFAULT_SETTINGS), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
						return;

					m_settings->Remove("ui");
					Reboot();
				});
		connect(m_ui.actionExportUserData,
		        &QAction::triggered,
		        &m_self,
		        [&]
		        {
					auto controller = ILogicFactory::Lock(m_logicFactory)->CreateUserDataController();
					controller->Backup([controller]() mutable { controller.reset(); });
				});
		connect(m_ui.actionImportUserData,
		        &QAction::triggered,
		        &m_self,
		        [&]
		        {
					auto controller = ILogicFactory::Lock(m_logicFactory)->CreateUserDataController();
					controller->Restore([controller]() mutable { controller.reset(); });
				});

		connect(m_ui.actionTestLogColors,
		        &QAction::triggered,
		        &m_self,
		        [&]
		        {
					if (!m_ui.actionShowLog->isChecked())
						m_ui.actionShowLog->trigger();

					m_logController->TestColors();
				});

		connect(m_navigationWidget.get(), &TreeView::NavigationModeNameChanged, m_booksWidget.get(), &TreeView::SetNavigationModeName);
		connect(m_ui.actionHideAnnotation, &QAction::visibleChanged, &m_self, [&] { m_ui.menuAnnotation->menuAction()->setVisible(m_ui.actionHideAnnotation->isVisible()); });
		connect(m_ui.actionShowJokes,
		        &QAction::triggered,
		        &m_self,
		        [this](const bool checked)
		        {
					m_annotationController->ShowJokes(checked);
					m_settings->Set(SHOW_JOKES_KEY, checked);
				});

		connect(m_ui.actionScripts, &QAction::triggered, &m_self, [&] { m_uiFactory->CreateScriptDialog()->Exec(); });

		connect(m_ui.actionOpds, &QAction::triggered, &m_self, [&] { m_uiFactory->CreateOpdsDialog()->exec(); });

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

		connect(m_ui.actionPermanentLanguageFilter, &QAction::triggered, &m_self, [&](const bool checked) { m_settings->Set(Constant::Settings::KEEP_RECENT_LANG_FILTER_KEY, checked); });
		connect(m_ui.actionGenerateIndexInpx, &QAction::triggered, &m_self, [&] { GenerateCollectionInpx(); });
		connect(m_ui.actionShowCollectionCleaner, &QAction::triggered, &m_self, [&] { m_uiFactory->CreateCollectionCleaner()->exec(); });

		ConnectShowHide(m_booksWidget.get(), &TreeView::ShowRemoved, m_ui.actionShowRemoved, m_ui.actionHideRemoved, SHOW_REMOVED_BOOKS_KEY);
		ConnectShowHide(m_ui.annotationWidget, &QWidget::setVisible, m_ui.actionShowAnnotation, m_ui.actionHideAnnotation, SHOW_ANNOTATION_KEY);
		ConnectShowHide(m_annotationWidget.get(), &AnnotationWidget::ShowContent, m_ui.actionShowAnnotationContent, m_ui.actionHideAnnotationContent, SHOW_ANNOTATION_CONTENT_KEY);
		ConnectShowHide(m_annotationWidget.get(), &AnnotationWidget::ShowCover, m_ui.actionShowAnnotationCover, m_ui.actionHideAnnotationCover, SHOW_ANNOTATION_COVER_KEY);
		ConnectShowHide(m_annotationWidget.get(), &AnnotationWidget::ShowCoverButtons, m_ui.actionShowAnnotationCoverButtons, m_ui.actionHideAnnotationCoverButtons, SHOW_ANNOTATION_COVER_BUTTONS_KEY);
		ConnectShowHide(this, &Impl::AllowDestructiveOperation, m_ui.actionAllowDestructiveOperations, m_ui.actionDenyDestructiveOperations);
		ConnectShowHide<QStatusBar>(m_ui.statusBar, &QWidget::setVisible, m_ui.actionShowStatusBar, m_ui.actionHideStatusBar, SHOW_STATUS_BAR_KEY);
		ConnectShowHide<QLineEdit>(m_ui.lineEditBookTitleToSearch, &QWidget::setVisible, m_ui.actionShowSearchBookString, m_ui.actionHideSearchBookString, SHOW_SEARCH_BOOK_KEY);

		connect(m_ui.lineEditBookTitleToSearch, &QLineEdit::returnPressed, &m_self, [this] { SearchBookByTitle(); });
		connect(m_ui.actionSearchBookByTitle, &QAction::triggered, &m_self, [this] { SearchBookByTitle(); });

		connect(m_ui.actionExternalThemeLoad, &QAction::triggered, &m_self, [this] { SetExternalStyle(m_uiFactory->GetOpenFileName(QSS, Tr(SELECT_QSS_FILE), Tr(QSS_FILE_FILTER).arg(QSS))); });

		CreateStylesMenu();
	}

	void CreateStylesMenu()
	{
		const auto setAction = [](QAction* action, const bool checked)
		{
			action->setChecked(checked);
			action->setEnabled(!checked);
		};

		const auto addActionGroup = [this, setAction]<typename T>(const std::vector<QAction*>& actions, const QString& key, const QString& defaultValue, T&& f, QActionGroup* group = nullptr) -> QActionGroup*
		{
			if (!group)
			{
				group = new QActionGroup(&m_self);
				group->setExclusive(true);
			}

			auto set = [this, setAction, group, key, defaultValue]
			{
				auto groupActions = group->actions();

				auto currentTheme = m_settings->Get(key, defaultValue);
				if (const auto it =
				        std::ranges::find_if(groupActions, [&](const QAction* action) { return action->property(ACTION_PROPERTY_NAME).toString().compare(currentTheme, Qt::CaseInsensitive) == 0; });
				    it != groupActions.end())
					setAction(*it, true);
			};

			const auto change = [this, set, key, f = std::forward<T>(f)](const QString& theme)
			{
				if (!f(theme))
					return;

				m_settings->Set(key, theme);
				set();
				RebootDialog();
			};

			for (auto* action : actions)
			{
				connect(action, &QAction::triggered, &m_self, [=] { change(action->property(ACTION_PROPERTY_NAME).toString()); });
				group->addAction(action);
			}

			set();

			return group;
		};

		std::vector<QAction*> pluginStyles;
		for (const auto& key : QStyleFactory::keys())
		{
			const auto* style = QStyleFactory::create(key);
			if (!style)
				continue;

			auto* action = pluginStyles.emplace_back(m_ui.menuTheme->addAction(style->name()));
			action->setProperty(ACTION_PROPERTY_NAME, key);
			action->setCheckable(true);
		}
		auto* group = addActionGroup(pluginStyles,
		                             Constant::Settings::THEME_KEY,
		                             Constant::Settings::APP_STYLE_DEFAULT,
		                             [this](const QString&) { return m_settings->Remove(Constant::Settings::EXTERNAL_THEME_KEY), true; });
		m_ui.menuTheme->addSeparator();

		std::vector<QAction*> externalStyles;
		for (const auto& entry : QDir(QApplication::applicationDirPath() + "/themes").entryInfoList(QStringList() << "*.qss" << "*.dll", QDir::Files))
		{
			const auto fileName = entry.filePath();
			auto* action = externalStyles.emplace_back(m_ui.menuTheme->addAction(entry.completeBaseName()));
			action->setProperty(ACTION_PROPERTY_NAME, fileName);
			action->setCheckable(true);
		}
		addActionGroup(externalStyles, Constant::Settings::EXTERNAL_THEME_KEY, {}, [this](const QString& fileName) { return SetExternalStyle(fileName), false; }, group);
		m_ui.menuTheme->addSeparator();
		m_ui.menuTheme->addAction(m_ui.actionExternalThemeLoad);
		group->addAction(m_ui.actionExternalThemeLoad);
		if (const auto currentExternalTheme = m_settings->Get(Constant::Settings::EXTERNAL_THEME_KEY); currentExternalTheme.isValid())
		{
			if (auto* action = group->checkedAction(); action && action->property(ACTION_PROPERTY_NAME) != currentExternalTheme)
			{
				setAction(action, false);
				m_ui.actionExternalThemeLoad->setChecked(true);
			}
		}

		m_ui.menuColorScheme->setEnabled(true);
		addActionGroup({ m_ui.actionColorSchemeSystem, m_ui.actionColorSchemeLight, m_ui.actionColorSchemeDark },
		               Constant::Settings::COLOR_SCHEME_KEY,
		               Constant::Settings::APP_COLOR_SCHEME_DEFAULT,
		               [](const QString&) { return true; });
	}

	void RebootDialog() const
	{
		if (m_uiFactory->ShowQuestion(Loc::Tr(Loc::Ctx::COMMON, Loc::CONFIRM_RESTART), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
			Reboot();
	}

	void SetExternalStyle(const QString& fileName)
	{
		if (fileName.isEmpty())
			return;

		if (QFileInfo(fileName).suffix().toLower() == "dll")
		{
			Util::DyLib lib(fileName.toStdString());
			QStringList list;
			QDirIterator it(":/", QStringList() << "*.qss", QDir::Files, QDirIterator::Subdirectories);
			while (it.hasNext())
				list << it.next();
			erase_if(list, [](const QString& item) { return item == Constant::STYLE_FILE_NAME; });

			if (const auto qss = m_uiFactory->GetText(Tr(EXTERNAL_STYLESHEET), Tr(EXTERNAL_STYLESHEET_FILE_NAME), list.isEmpty() ? QString {} : list.front(), list); !qss.isEmpty())
				m_settings->Set(Constant::Settings::EXTERNAL_THEME_QSS_KEY, qss);
			else
				return;
		}

		m_settings->Set(Constant::Settings::EXTERNAL_THEME_KEY, fileName);
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

	template <typename T>
	static void OnObjectVisibleChanged(T* obj, void (T::*f)(bool), QAction* show, QAction* hide, const bool value)
	{
		((*obj).*f)(value);
		hide->setVisible(value);
		show->setVisible(!value);
	}

	template <typename T>
	void ConnectShowHide(T* obj, void (T::*f)(bool), QAction* show, QAction* hide, const char* key = nullptr)
	{
		const auto showHide = [=, &settings = *m_settings](const bool value)
		{
			if (key)
				settings.Set(key, value);
			Impl::OnObjectVisibleChanged<T>(obj, f, show, hide, value);
		};

		connect(show, &QAction::triggered, &m_self, [=] { showHide(true); });
		connect(hide, &QAction::triggered, &m_self, [=] { showHide(false); });
	}

	void GenerateCollectionInpx() const
	{
		auto inpxGenerator = ILogicFactory::Lock(m_logicFactory)->CreateInpxGenerator();
		auto& inpxGeneratorRef = *inpxGenerator;
		if (auto inpxFileName = m_uiFactory->GetSaveFileName(Constant::Settings::EXPORT_DIALOG_KEY, Loc::Tr(Loc::EXPORT, Loc::SELECT_INPX_FILE), Loc::Tr(Loc::EXPORT, Loc::SELECT_INPX_FILE_FILTER));
		    !inpxFileName.isEmpty())
			inpxGeneratorRef.GenerateInpx(std::move(inpxFileName), [inpxGenerator = std::move(inpxGenerator)](bool) mutable { inpxGenerator.reset(); });
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
};

MainWindow::MainWindow(const std::shared_ptr<const ILogicFactory>& logicFactory,
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

void MainWindow::OnBooksSearchFilterValueGeometryChanged(const QRect& geometry)
{
	m_impl->OnBooksSearchFilterValueGeometryChanged(geometry);
}
