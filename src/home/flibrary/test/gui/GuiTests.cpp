// GUI tests: the real application stack runs in-process, wired by the same DI container FLibrary builds,
// on throw-away collections in a temporary settings folder, and the main window is driven through Qt Test.

#include <QAbstractItemModelTester>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QTranslator>
#include <QTreeView>

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/SettingsConstant.h"
#include "interface/localization.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/IDatabaseController.h"
#include "interface/logic/IDatabaseMigrator.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/ITaskQueue.h"
#include "interface/logic/ITreeViewController.h"
#include "interface/ui/IMainWindow.h"

#include "Hypodermic/Hypodermic.h"
#include "djvu/djvu.h"
#include "genre/Genre.h"
#include "gui/di_ui.h"
#include "gutil/di_gutil.h"
#include "logging/init.h"
#include "logic/data/DataItem.h"
#include "logic/di_logic.h"
#include "logic/model/LogModel.h"
#include "settings/ISettings.h"
#include "util/xml/Initializer.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace HomeCompa::Flibrary;

namespace {

// Upper bound for anything asynchronous: collection indexing, navigation queries, book lists
constexpr auto WAIT_MS = 10000;

constexpr auto EMPTY_COLLECTION = "Empty library";
constexpr auto BOOKS_COLLECTION = "Test library";

// Text the fixture library (tool/opds/test/make_fixture.py) is known to contain
constexpr auto AUTHOR_PETROV              = "Petrov Ivan";
constexpr auto AUTHOR_BROWN               = "Brown Lee"; // last and middle name, no first name
constexpr auto PETROV_BOOK                = "First Test Book";
constexpr auto PETROV_BOOK_2              = "Second Test Book";
constexpr auto OTHER_AUTHOR_BOOK          = "Detective Story";
constexpr auto BROWN_BOOK                 = "Lost Letters"; // stored as "O'Brien - Story.fb2"
constexpr auto SERIES_SAGA                = "Test Saga";
constexpr auto LANGUAGE_GERMAN            = "German";
constexpr auto GROUP_TITLE                = "Test group";
constexpr auto REMOVE_FROM_GROUP_KEY      = "Remove from group"; // menu item keys are the untranslated titles
constexpr auto GENRE_DETECTIVES           = "Detectives";
constexpr auto GENRE_CLASSICAL_DETECTIVES = "Classical detectives";
constexpr auto SEARCH_TITLE               = "zzqx no such title"; // matches no book title, only the file name
constexpr auto SEARCH_ORIGIN              = "/library/O'Brien - Story.fb2"; // the navigation list shows a search by its origin

constexpr auto MODE_PROPERTY_NAME = "modeName";

using Translators = decltype(Loc::LoadLocales(std::declval<const ISettings&>()));

// Same as the app's DiInit. Some registrations capture `container` itself by reference,
// so the caller's variable must stay alive for as long as the container is used
void BuildContainer(std::shared_ptr<Hypodermic::Container>& container)
{
	Hypodermic::ContainerBuilder builder;
	DiLogic(builder, container);
	DiUi(builder, container);
	Util::DiGUtil(builder, container);
	container = builder.build();
}

// Ends a local event loop once the collection controller reports a new active collection
class ActiveCollectionWaiter final : public ICollectionsObserver
{
public:
	explicit ActiveCollectionWaiter(QEventLoop& eventLoop)
		: m_eventLoop { eventLoop }
	{
	}

private: // ICollectionsObserver
	void OnActiveCollectionChanged() override
	{
		m_eventLoop.exit(0);
	}

	void OnNewCollectionCreating(bool) override
	{
	}

private:
	QEventLoop& m_eventLoop;
};

// One TreeView panel of the main window (navigation or books), found the way an outside tool would: by object and accessible names
struct Panel
{
	QTreeView*   tree { nullptr };
	QToolButton* modeButton { nullptr };
};

Panel FindPanel(const QString& itemName)
{
	const auto accessibleName = QString("%1SearchAndFilter").arg(itemName);
	for (auto* topLevel : QApplication::topLevelWidgets())
		for (auto* value : topLevel->findChildren<QLineEdit*>("value"))
		{
			if (value->accessibleName() != accessibleName)
				continue;

			auto* root = value->parentWidget();
			while (root && root->objectName() != "TreeView")
				root = root->parentWidget();
			if (!root)
				continue;

			return { root->findChild<QTreeView*>("treeView"), root->findChild<QToolButton*>("btnMode") };
		}

	return {};
}

bool ContainsText(const QAbstractItemModel& model, const QString& text, const QModelIndex& parent = {})
{
	for (int row = 0, rows = model.rowCount(parent); row < rows; ++row)
		for (int column = 0, columns = model.columnCount(parent); column < columns; ++column)
		{
			const auto index = model.index(row, column, parent);
			if (index.data(Qt::DisplayRole).toString().contains(text))
				return true;
			if (column == 0 && model.hasChildren(index) && ContainsText(model, text, index))
				return true;
		}

	return false;
}

// What a panel shows, for failure messages: the first rows with their children, column 0
QString DescribeModel(const QAbstractItemModel* model, const QModelIndex& parent = {}, int depth = 0)
{
	if (!model)
		return "no model";

	QStringList lines;
	if (depth == 0)
		lines << QString("%1 with %2 rows:").arg(model->metaObject()->className()).arg(model->rowCount());
	for (int row = 0, rows = std::min(model->rowCount(parent), 8); row < rows; ++row)
	{
		const auto index = model->index(row, 0, parent);
		lines << QString(depth * 2 + 2, ' ') + "'" + index.data(Qt::DisplayRole).toString() + "'";
		if (depth < 2 && model->hasChildren(index))
			lines << DescribeModel(model, index, depth + 1);
	}
	return lines.join('\n');
}

QModelIndex FindText(const QAbstractItemModel& model, const QString& text)
{
	const auto found = model.match(model.index(0, 0), Qt::DisplayRole, text, 1, Qt::MatchContains | Qt::MatchRecursive);
	return found.isEmpty() ? QModelIndex {} : found.front();
}

QAction* FindModeAction(const QToolButton& modeButton, const QString& modeName)
{
	for (auto* action : modeButton.actions())
		if (action->property(MODE_PROPERTY_NAME).toString() == modeName)
			return action;
	return nullptr;
}

// The app reports results in modal dialogs (e.g. the summary after indexing a collection), which would leave a test waiting
// for a click. This closes each one as it opens, logging its text: a message box gets its default button (which is "No" for
// destructive questions), any other dialog is cancelled. The timer also fires inside the dialog's own event loop.
class ModalDialogCloser final : public QObject
{
public:
	ModalDialogCloser()
	{
		connect(&m_timer, &QTimer::timeout, this, [] {
			CloseActiveModalDialog();
		});
		m_timer.start(std::chrono::milliseconds(50));
	}

private:
	static void CloseActiveModalDialog()
	{
		auto* widget = QApplication::activeModalWidget();
		if (!widget)
			return;

		if (auto* messageBox = qobject_cast<QMessageBox*>(widget))
		{
			qInfo("closing message box: %s", qPrintable(messageBox->text()));
			auto* button = messageBox->defaultButton() ? static_cast<QAbstractButton*>(messageBox->defaultButton()) : messageBox->button(QMessageBox::Ok);
			if (button)
				button->click();
			else
				messageBox->reject();
			return;
		}

		if (auto* dialog = qobject_cast<QDialog*>(widget))
		{
			qInfo("cancelling dialog: %s", qPrintable(dialog->windowTitle()));
			dialog->reject();
		}
	}

private:
	QTimer m_timer;
};

} // namespace

class GuiTests final : public QObject
{
	Q_OBJECT

public:
	explicit GuiTests(QString root)
		: m_root { std::move(root) }
	{
	}

private slots:

	void initTestCase()
	{
		const auto python = QStandardPaths::findExecutable("python3");
		QVERIFY2(!python.isEmpty(), "python3 is needed to generate the test library");

		QVERIFY(QDir().mkpath(ArchivesFolder(EMPTY_COLLECTION)));
		QCOMPARE(QProcess::execute(python, { "-I", FLIBRARY_TEST_FIXTURE_SCRIPT, ArchivesFolder(BOOKS_COLLECTION) }), 0);

		CreateCollection(EMPTY_COLLECTION);
		CreateCollection(BOOKS_COLLECTION);
	}

	void cleanup()
	{
		Stop();
	}

	// An empty collection offers no navigation mode at all; the main window must still come up and stay up
	void mainWindowStartsWithEmptyCollection()
	{
		ActivateCollection(EMPTY_COLLECTION);
		Start();

		const auto navigation = FindPanel("Navigation");
		QVERIFY(navigation.tree);
		QVERIFY(navigation.modeButton);
		QTest::qWait(500); // let the deferred start-up work run (mode name, button icon)
		QCOMPARE(navigation.tree->model() ? navigation.tree->model()->rowCount() : 0, 0);
	}

	void navigationListsTheCollectionAuthors()
	{
		ActivateCollection(BOOKS_COLLECTION);
		Start();

		const auto navigation = FindPanel("Navigation");
		ShowAuthors(navigation);
		if (QTest::currentTestFailed())
			return;

		QAbstractItemModelTester tester(navigation.tree->model(), QAbstractItemModelTester::FailureReportingMode::QtTest);
	}

	void navigationSwitchesBetweenModes()
	{
		ActivateCollection(BOOKS_COLLECTION);
		Start();

		const auto navigation = FindPanel("Navigation");
		ShowAuthors(navigation);
		if (QTest::currentTestFailed())
			return;

		auto* genres = FindModeAction(*navigation.modeButton, "Genres");
		QVERIFY2(genres, "the test library must offer the Genres mode");
		genres->trigger();
		QTRY_VERIFY2_WITH_TIMEOUT(
			navigation.tree->model() && navigation.tree->model()->rowCount() > 0 && !ContainsText(*navigation.tree->model(), AUTHOR_PETROV),
			qPrintable(DescribeModel(navigation.tree->model())),
			WAIT_MS
		);
		{
			QAbstractItemModelTester tester(navigation.tree->model(), QAbstractItemModelTester::FailureReportingMode::QtTest);
		}

		ShowAuthors(navigation);
	}

	void selectingAnAuthorListsOnlyTheirBooks()
	{
		ActivateCollection(BOOKS_COLLECTION);
		Start();

		const auto navigation = FindPanel("Navigation");
		const auto books      = FindPanel("Books");
		QVERIFY(books.tree);
		ShowAuthors(navigation);
		if (QTest::currentTestFailed())
			return;

		navigation.tree->setCurrentIndex(FindText(*navigation.tree->model(), AUTHOR_PETROV));
		QTRY_VERIFY2_WITH_TIMEOUT(books.tree->model() && ContainsText(*books.tree->model(), PETROV_BOOK), qPrintable(DescribeModel(books.tree->model())), WAIT_MS);
		QVERIFY2(!ContainsText(*books.tree->model(), OTHER_AUTHOR_BOOK), qPrintable(DescribeModel(books.tree->model())));

		QAbstractItemModelTester tester(books.tree->model(), QAbstractItemModelTester::FailureReportingMode::QtTest);
	}

	// The author's display name is built in SQL; a missing first name must not drop the middle name
	void authorWithoutFirstNameKeepsMiddleName()
	{
		ActivateCollection(BOOKS_COLLECTION);
		Start();

		const auto navigation = FindPanel("Navigation");
		ShowAuthors(navigation);
		if (QTest::currentTestFailed())
			return;

		QVERIFY2(ContainsText(*navigation.tree->model(), AUTHOR_BROWN), qPrintable(DescribeModel(navigation.tree->model())));
	}

	void selectingASeriesListsOnlyItsBooks()
	{
		ActivateCollection(BOOKS_COLLECTION);
		Start();

		const auto navigation = FindPanel("Navigation");
		ShowMode(navigation, "Series", SERIES_SAGA);
		if (QTest::currentTestFailed())
			return;

		ExpectBooks(navigation, FindText(*navigation.tree->model(), SERIES_SAGA), { PETROV_BOOK, PETROV_BOOK_2 }, { OTHER_AUTHOR_BOOK, BROWN_BOOK });
	}

	// Genre codes are text values in the navigation queries
	void selectingAGenreListsOnlyItsBooks()
	{
		ActivateCollection(BOOKS_COLLECTION);
		Start();

		const auto navigation = FindPanel("Navigation");
		ShowMode(navigation, "Genres", GENRE_DETECTIVES);
		if (QTest::currentTestFailed())
			return;

		ExpectBooks(navigation, FindText(*navigation.tree->model(), GENRE_CLASSICAL_DETECTIVES), { OTHER_AUTHOR_BOOK }, { PETROV_BOOK, BROWN_BOOK });
	}

	// Language codes are text values in the navigation queries too; only the Brown book is in German
	void selectingALanguageListsItsBooks()
	{
		ActivateCollection(BOOKS_COLLECTION);
		Start();

		const auto navigation = FindPanel("Navigation");
		ShowMode(navigation, "Languages", LANGUAGE_GERMAN);
		if (QTest::currentTestFailed())
			return;

		ExpectBooks(navigation, FindText(*navigation.tree->model(), LANGUAGE_GERMAN), { BROWN_BOOK }, { PETROV_BOOK, OTHER_AUTHOR_BOOK });
	}

	// Search by file name compares against the search's origin file name; an apostrophe in it must not break the query
	void searchByFileNameFindsAFileWithAnApostrophe()
	{
		ActivateCollection(BOOKS_COLLECTION);
		AddSearchByFileName(SEARCH_TITLE, SEARCH_ORIGIN);
		if (QTest::currentTestFailed())
			return;
		Start();

		const auto navigation = FindPanel("Navigation");
		ShowMode(navigation, "Search", SEARCH_ORIGIN);
		if (QTest::currentTestFailed())
			return;

		ExpectBooks(navigation, FindText(*navigation.tree->model(), SEARCH_ORIGIN), { BROWN_BOOK }, { PETROV_BOOK });
	}

	// Making a book current records it in the history (a bound book id), which the History mode then lists.
	// The history starts empty with each run of the app; no test makes the Smith book current
	void currentBookIsListedInHistory()
	{
		ActivateCollection(BOOKS_COLLECTION);
		Start();

		const auto navigation = FindPanel("Navigation");
		const auto books      = FindPanel("Books");
		ShowAuthors(navigation);
		if (QTest::currentTestFailed())
			return;

		ExpectBooks(navigation, FindText(*navigation.tree->model(), AUTHOR_BROWN), { BROWN_BOOK }, { PETROV_BOOK });
		if (QTest::currentTestFailed())
			return;
		books.tree->setCurrentIndex(FindText(*books.tree->model(), BROWN_BOOK));
		// The history is written once the book's annotation is loaded, and the History mode does not pick up later writes
		QTRY_VERIFY_WITH_TIMEOUT(IsInHistory(BROWN_BOOK), WAIT_MS);

		auto* history = FindModeAction(*navigation.modeButton, "History");
		QVERIFY2(history, "the app must always offer the History mode");
		history->trigger();
		// The History mode has a single navigation item without text; the Authors list it replaces has three
		QTRY_VERIFY2_WITH_TIMEOUT(navigation.tree->model() && navigation.tree->model()->rowCount() == 1, qPrintable(DescribeModel(navigation.tree->model())), WAIT_MS);

		ExpectBooks(navigation, navigation.tree->model()->index(0, 0), { BROWN_BOOK }, { OTHER_AUTHOR_BOOK });
	}

	// The group's "Remove from group" menu lists its authors by name with the author id as the item's parameter, and
	// choosing an author removes it from the group. All three are compared at once, so a failure shows every symptom.
	void groupMenuRemovesAnAuthorFromTheGroup()
	{
		ActivateCollection(BOOKS_COLLECTION);
		long long authorId = 0;
		AddGroupWithAuthor(GROUP_TITLE, "Brown", authorId);
		if (QTest::currentTestFailed())
			return;
		Start();

		const auto navigation = FindPanel("Navigation");
		ShowMode(navigation, "Groups", GROUP_TITLE);
		if (QTest::currentTestFailed())
			return;

		const auto group      = FindText(*navigation.tree->model(), GROUP_TITLE);
		const auto controller = m_container->resolve<ILogicFactory>()->GetTreeViewController(ItemType::Navigation);

		// The menu arrives on the main thread later; shared, so a late callback after a failed wait has somewhere to write
		const auto menu = std::make_shared<IDataItem::Ptr>();
		controller->RequestContextMenu(group, ITreeViewController::RequestContextMenuOptions::HasSelection, [menu](const QString&, const IDataItem::Ptr& item) {
			*menu = item;
		});
		QTRY_VERIFY_WITH_TIMEOUT(*menu, WAIT_MS);

		const auto removeFromGroup = FindMenuChild(*menu, REMOVE_FROM_GROUP_KEY);
		QVERIFY2(removeFromGroup, "the group menu must offer Remove from group");
		const auto authors = FindMenuChild(removeFromGroup, "Authors");
		QVERIFY2(authors && authors->GetChildCount() == 1, "Remove from group must list the group's one author");
		const auto author = authors->GetChild(0);

		QVERIFY2(IsInGroup(GROUP_TITLE, authorId), "the author must be in the group before it is removed");

		// Choose the author, as a click on the menu item does
		controller->OnContextMenuTriggered(navigation.tree->model(), group, { group }, author);
		const auto removed = QTest::qWaitFor(
			[&] {
				return !IsInGroup(GROUP_TITLE, authorId);
			},
			WAIT_MS
		);

		const auto describe = [](const QString& name, const QString& id, const bool inGroup) {
			return QString("name '%1', id %2, %3").arg(name, id, inGroup ? "still in the group" : "removed from the group");
		};
		QCOMPARE(describe(author->GetData(MenuItem::Column::Title), author->GetData(MenuItem::Column::Parameter), !removed), describe(AUTHOR_BROWN, QString::number(authorId), false));
	}

private:
	// The navigation mode is a global setting that an earlier test may have changed, so each test chooses its own
	// Waits for an item of the new mode: until the mode's model is ready, the view still shows the previous mode's items
	static void ShowMode(const Panel& navigation, const char* modeName, const QString& expectedText)
	{
		QVERIFY(navigation.tree && navigation.modeButton);
		auto* action = FindModeAction(*navigation.modeButton, modeName);
		QVERIFY2(action, qPrintable(QString("the test library must offer the %1 mode").arg(modeName)));
		action->trigger();
		QTRY_VERIFY2_WITH_TIMEOUT(navigation.tree->model() && ContainsText(*navigation.tree->model(), expectedText), qPrintable(DescribeModel(navigation.tree->model())), WAIT_MS);
	}

	static void ShowAuthors(const Panel& navigation)
	{
		ShowMode(navigation, "Authors", AUTHOR_PETROV);
	}

	// Selects a navigation item and checks the books panel lists exactly the expected titles among the given ones
	static void ExpectBooks(const Panel& navigation, const QModelIndex& item, const QStringList& present, const QStringList& absent)
	{
		const auto books = FindPanel("Books");
		QVERIFY(books.tree);
		QVERIFY2(item.isValid(), qPrintable(DescribeModel(navigation.tree->model())));

		navigation.tree->setCurrentIndex(item);
		QTRY_VERIFY2_WITH_TIMEOUT(
			books.tree->model()
				&& std::ranges::all_of(
					present,
					[&](const QString& title) {
						return ContainsText(*books.tree->model(), title);
					}
				),
			qPrintable(DescribeModel(books.tree->model())),
			WAIT_MS
		);
		for (const auto& title : absent)
			QVERIFY2(!ContainsText(*books.tree->model(), title), qPrintable(QString("'%1' must not be listed:\n%2").arg(title, DescribeModel(books.tree->model()))));

		QAbstractItemModelTester tester(books.tree->model(), QAbstractItemModelTester::FailureReportingMode::QtTest);
	}

	// Stores a search the way the search dialog does, with "search by file name" switched on
	static void AddSearchByFileName(const QString& title, const QString& origin)
	{
		std::shared_ptr<Hypodermic::Container> container;
		BuildContainer(container);
		container->resolve<ISettings>()->Set(Constant::Settings::SEARCH_WITH_FILENAME, true);

		const auto db          = container->resolve<IDatabaseController>()->GetDatabase();
		const auto transaction = db->CreateTransaction();
		QVERIFY(transaction->CreateCommand("insert into Searches_User(Title, Origin, CreatedAt) values(?, ?, datetime('now'))")->Execute(title.toStdString(), origin.toStdString()));
		QVERIFY(transaction->Commit());
	}

	// Asks the running app's database; the unqualified name also finds the default history table in the attached in-memory "tmp" database
	bool IsInHistory(const QString& title) const
	{
		const auto db    = m_container->resolve<IDatabaseController>()->GetDatabase();
		const auto query = db->CreateQuery("select exists (select 42 from History h join Books b on b.BookID = h.BookID where b.Title = ?)");
		query->Bind(0, title.toStdString());
		query->Execute();
		return !query->Eof() && query->Get<int>(0) != 0;
	}

	// Creates a group holding the author with the given last name, the way "add to group" stores it
	static void AddGroupWithAuthor(const QString& title, const QString& authorLastName, long long& authorId)
	{
		std::shared_ptr<Hypodermic::Container> container;
		BuildContainer(container);

		const auto db          = container->resolve<IDatabaseController>()->GetDatabase();
		const auto transaction = db->CreateTransaction();
		const auto author      = transaction->CreateQuery("select AuthorID from Authors where LastName = ?");
		author->Bind(0, authorLastName.toStdString());
		author->Execute();
		QVERIFY2(!author->Eof(), "the test library must contain the author");
		authorId = author->Get<long long>(0);

		QVERIFY(transaction->CreateCommand("insert into Groups_User(Title, CreatedAt) values(?, datetime('now'))")->Execute(title.toStdString()));
		QVERIFY(
			transaction->CreateCommand("insert into Groups_List_User(GroupID, ObjectID, CreatedAt) select GroupID, ?, datetime('now') from Groups_User where Title = ?")->Execute(authorId, title.toStdString())
		);
		QVERIFY(transaction->Commit());
	}

	// Asks the running app's database whether the group holds the object
	bool IsInGroup(const QString& title, const long long objectId) const
	{
		const auto db    = m_container->resolve<IDatabaseController>()->GetDatabase();
		const auto query = db->CreateQuery("select exists (select 42 from Groups_List_User l join Groups_User g on g.GroupID = l.GroupID where g.Title = ? and l.ObjectID = ?)");
		query->Bind(0, title.toStdString());
		query->Bind(1, objectId);
		query->Execute();
		return !query->Eof() && query->Get<int>(0) != 0;
	}

	// The child menu item with the given key, or null
	static IDataItem::Ptr FindMenuChild(const IDataItem::Ptr& menu, const QString& key)
	{
		for (size_t i = 0, sz = menu->GetChildCount(); i < sz; ++i)
			if (auto child = menu->GetChild(i); child->GetData(MenuItem::Column::Key) == key)
				return child;
		return {};
	}

	QString ArchivesFolder(const QString& collection) const
	{
		return QDir(m_root).filePath(QString(collection).replace(' ', '_'));
	}

	// Creates and indexes a collection with its own container, as the "new collection" dialog does, then drops that container
	void CreateCollection(const QString& name)
	{
		std::shared_ptr<Hypodermic::Container> container;
		BuildContainer(container);
		const auto controller = container->resolve<ICollectionController>();

		QEventLoop             eventLoop;
		ActiveCollectionWaiter waiter(eventLoop);
		controller->RegisterObserver(&waiter);

		const auto collection = controller->CreateCollection(name, ArchivesFolder(name) + ".db", ArchivesFolder(name), {}, {});
		controller->CreateCollection(*collection);
		QTimer::singleShot(WAIT_MS, &eventLoop, [&] {
			eventLoop.exit(1);
		});
		const auto timedOut = eventLoop.exec() != 0;

		controller->UnregisterObserver(&waiter);
		QVERIFY2(!timedOut, qPrintable(QString("creating collection '%1' timed out").arg(name)));
	}

	void ActivateCollection(const QString& name)
	{
		std::shared_ptr<Hypodermic::Container> container;
		BuildContainer(container);
		const auto  controller  = container->resolve<ICollectionController>();
		const auto& collections = controller->GetCollections();
		const auto  it          = std::ranges::find(collections, name, [](const auto& collection) {
            return collection->name;
        });
		QVERIFY2(it != collections.end(), qPrintable(QString("collection '%1' was not created").arg(name)));
		if (!controller->ActiveCollectionExists() || controller->GetActiveCollectionId() != (*it)->id)
			controller->SetActiveCollection((*it)->id);
	}

	// Same start-up sequence as main.cpp, minus the single-instance guard and the theme
	void Start()
	{
		BuildContainer(m_container);
		const auto settings = m_container->resolve<ISettings>();
		m_translators       = Loc::LoadLocales(*settings);
		Genre::SetSortMode(*settings);

		QCOMPARE(m_container->resolve<IDatabaseMigrator>()->NeedMigrate(), IDatabaseMigrator::NeedMigrateResult::Actual);

		m_container->resolve<ITaskQueue>()->Execute();
		m_mainWindow = m_container->resolve<IMainWindow>();
		m_mainWindow->Show();
	}

	void Stop()
	{
		m_mainWindow.reset();
		m_translators.clear();
		m_container.reset();
		QTest::qWait(100); // let deleteLater() and queued work of the closed window run before the next test

		for (const auto* widget : QApplication::topLevelWidgets())
			if (widget->findChild<QTreeView*>("treeView"))
				qWarning("a %s with tree panels outlived the test, visible=%d", widget->metaObject()->className(), widget->isVisible());
	}

private:
	const QString                          m_root;
	std::shared_ptr<Hypodermic::Container> m_container;
	Translators                            m_translators;
	std::shared_ptr<IMainWindow>           m_mainWindow;
};

int main(int argc, char* argv[])
{
	// Everything the app writes (settings, collections, logs) goes to a temporary folder; no display is needed
	QTemporaryDir root(QDir(QDir::tempPath()).filePath("flib-gui-tests-XXXXXX"));
	if (!root.isValid())
		return 1;
	qputenv("XDG_CONFIG_HOME", QDir(root.path()).filePath("config").toUtf8());
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", "offscreen");

	QApplication app(argc, argv);
	QCoreApplication::setApplicationName(PRODUCT_ID);
	Util::XMLPlatformInitializer xmlPlatformInitializer;
	DjVu::Initializer            djvuInitializer;
	Log::LoggingInitializer      logging(QDir(root.path()).filePath("gui_tests.log"));
	LogModelAppender             logModelAppender; // the main window's log view needs it, as in main.cpp
	ModalDialogCloser            modalDialogCloser;

	GuiTests   tests(root.path());
	const auto result = QTest::qExec(&tests, argc, argv);
	if (result != 0)
	{
		root.setAutoRemove(false);
		qWarning("test folder kept for inspection: %s", qPrintable(root.path()));
	}
	return result;
}

#include "GuiTests.moc"
