// GUI tests: the real application stack runs in-process, wired by the same DI container FLibrary builds,
// on throw-away collections in a temporary settings folder, and the main window is driven through Qt Test.

#include <set>

#include <QAbstractItemModelTester>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
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

#include "interface/constants/ModelRole.h"
#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/localization.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/IDatabaseController.h"
#include "interface/logic/IDatabaseMigrator.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/ITaskQueue.h"
#include "interface/logic/ITreeViewController.h"
#include "interface/logic/IUserDataController.h"
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

// Upper bound for anything asynchronous: collection indexing, navigation queries, book lists.
// FLIBRARY_TEST_WAIT_MS raises it for slow runs, such as sanitizer builds
const int WAIT_MS = std::max(10000, qEnvironmentVariableIntValue("FLIBRARY_TEST_WAIT_MS"));

constexpr auto EMPTY_COLLECTION  = "Empty library";
constexpr auto BOOKS_COLLECTION  = "Test library";
constexpr auto UPDATE_COLLECTION = "Update library"; // the collection update test adds a book to it

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
constexpr auto REMOVE_FROM_GROUP_KEY      = "Remove from group"; // menu item keys are the untranslated titles without & marks
constexpr auto GENRE_DETECTIVES           = "Detectives";
constexpr auto GENRE_CLASSICAL_DETECTIVES = "Classical detectives";
constexpr auto SEARCH_TITLE               = "zzqx no such title"; // matches no book title, only the file name
constexpr auto SEARCH_ORIGIN              = "/library/O'Brien - Story.fb2"; // the navigation list shows a search by its origin
constexpr auto AUTHOR_NIKOLAEV            = "Nikolaev Petr"; // indexed as "Nikolaev [nick]": the nickname is not part of the name
constexpr auto NICKNAME                   = "nick";
constexpr auto PAPER_BOOK                 = "Paper Pages"; // Nikolaev, series "Leaves"
constexpr auto EPUB_BOOK                  = "Electronic Pages"; // Nikolaev, series "Pages", an ePub
constexpr auto UPDATE_BOOK                = "Late Pages"; // Nikolaev, added by the collection update
constexpr auto SERIES_PAGES               = "Pages";
constexpr auto FB2_ANNOTATION             = "Annotation of First Test Book.";
constexpr auto FB2_PUBLISHER              = "Test Press";
constexpr auto EPUB_ANNOTATION            = "Annotation of Electronic Pages.";
constexpr auto EPUB_PUBLISHER             = "Test Publisher";
constexpr auto EXPORT_ENCODING            = "windows-1251";
constexpr auto CP1251_TEST                = "\xD2\xE5\xF1\xF2"; // the fixture's body text has the Cyrillic word "Test", here in windows-1251
constexpr auto BOOKS_COLUMN               = "Format"; // a books column that every navigation mode shows
constexpr auto BACKUP_GROUP               = "Backup group";
constexpr auto BACKUP_SEARCH              = "Backup search";

// Settings the app reads from keys private to its sources
constexpr auto READ_MARK_COLOR_KEY        = "Preferences/ReadMark/color";
constexpr auto READ_MARK_BACKGROUND_KEY   = "Preferences/ReadMark/backgroundColor";
constexpr auto READ_MARK_WIDTH_KEY        = "Preferences/ReadMark/width";
constexpr auto READ_MARK_POSITION_KEY     = "Preferences/ReadMark/position";
constexpr auto COMMON_COLUMN_SETTINGS_KEY = "Preferences/CommonBooksTableColumnSettings";
constexpr auto EXPORT_ENCODING_KEY        = "Preferences/Export/Encoding/export";
constexpr auto CHECK_FOR_UPDATE_KEY       = "ui/View/CheckForUpdateOnStart"; // the app asks GitHub for a newer release at start-up
constexpr auto MODE_HIDDEN_KEY            = "ui/MenuCustomization/MainWindow/menuBar/menuNavigation/%1_isActive/hidden"; // All books and Already read are hidden by default

// A read mark colour is stored as 0xTTBBGGRR, with transparency instead of alpha: opaque magenta for the mark, and half
// transparent cyan for the background, which is painted over the row
constexpr uint READ_MARK_COLOR      = 0x00FF00FF;
constexpr uint READ_MARK_BACKGROUND = 0x7FFFFF00;
constexpr int  READ_MARK_WIDTH      = 4;

// Menu item keys are the untranslated titles without their & shortcut marks
constexpr auto EXPORT_KEY        = "Export";
constexpr auto EXPORT_AS_IS_KEY  = "As original format";
constexpr auto EXPORT_AS_ZIP_KEY = "As zip archive";

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
// destructive questions), a folder dialog gets the folder a test asked for with AnswerFolderDialog, any other dialog is
// cancelled. The timer also fires inside the dialog's own event loop.
class ModalDialogCloser final : public QObject
{
public:
	ModalDialogCloser()
	{
		connect(&m_timer, &QTimer::timeout, this, [this] {
			// A dialog is answered on the second look. The first can come while exec() is still showing it, before its event
			// loop runs; a dialog closed then leaves exec() waiting for good (seen with the folder dialog)
			auto* widget = QApplication::activeModalWidget();
			if (widget && widget == m_seen)
				CloseModalDialog(widget);
			m_seen = widget;
		});
		m_timer.start(std::chrono::milliseconds(50));
	}

	// The folder that the next folder dialogs choose, until it is reset with an empty string
	static void AnswerFolderDialog(QString folder)
	{
		FolderAnswer() = std::move(folder);
	}

private:
	static QString& FolderAnswer()
	{
		static QString folder;
		return folder;
	}

	static void CloseModalDialog(QWidget* widget)
	{
		if (auto* fileDialog = qobject_cast<QFileDialog*>(widget); fileDialog && !FolderAnswer().isEmpty())
		{
			qInfo("choosing %s in dialog: %s", qPrintable(FolderAnswer()), qPrintable(fileDialog->windowTitle()));
			fileDialog->setDirectory(FolderAnswer()); // in a folder dialog, the folder shown is the one chosen
			static_cast<QDialog*>(fileDialog)->done(QDialog::Accepted); // QFileDialog's override of done() is protected
			return;
		}

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
	QTimer            m_timer;
	QPointer<QWidget> m_seen; // the modal dialog of the previous look
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
		QCOMPARE(QProcess::execute(python, { "-I", FLIBRARY_TEST_FIXTURE_SCRIPT, ArchivesFolder(UPDATE_COLLECTION) }), 0);

		// The tests must not depend on the network
		{
			std::shared_ptr<Hypodermic::Container> container;
			BuildContainer(container);
			container->resolve<ISettings>()->Set(CHECK_FOR_UPDATE_KEY, false);
		}

		CreateCollection(EMPTY_COLLECTION);
		CreateCollection(UPDATE_COLLECTION);
		CreateCollection(BOOKS_COLLECTION);
	}

	void cleanup()
	{
		Stop();
		ModalDialogCloser::AnswerFolderDialog({});

		// Settings are shared by all tests: take back the ones this test changed
		if (m_changedSettings.isEmpty())
			return;
		std::shared_ptr<Hypodermic::Container> container;
		BuildContainer(container);
		const auto settings = container->resolve<ISettings>();
		for (const auto& key : m_changedSettings)
			settings->Remove(key);
		m_changedSettings.clear();
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
		// The History mode has a single navigation item without text; the Authors list it replaces has four
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

	// Opens every navigation mode the library offers and selects its first item, so that each navigation query and its
	// books query run at least once. A Debug build also asserts that no query reads a column its result does not have
	void everyNavigationModeShowsItsItems()
	{
		ActivateCollection(BOOKS_COLLECTION);
		ShowHiddenModes();
		Start();

		const auto navigation = FindPanel("Navigation");
		const auto books      = FindPanel("Books");
		QVERIFY(books.tree);
		ShowAuthors(navigation);
		if (QTest::currentTestFailed())
			return;

		// Modes whose first item has books in the test library; the others may be empty, depending on what earlier tests stored
		const QStringList modesWithBooks { "Authors", "Series", "Genres", "Keywords", "Archives", "Languages", "AllBooks" };
		QStringList       shown;
		for (auto* action : navigation.modeButton->actions())
		{
			const auto modeName = action->property(MODE_PROPERTY_NAME).toString();
			if (modeName.isEmpty())
				continue;
			const auto* previous      = navigation.tree->model();
			const auto  alreadyActive = action->isChecked();
			TriggerNavigationMenu(modeName);
			QTRY_VERIFY2_WITH_TIMEOUT(
				navigation.tree->model() && (alreadyActive || navigation.tree->model() != previous),
				qPrintable(QString("%1: the mode's list did not appear, still %2").arg(modeName, DescribeModel(navigation.tree->model()))),
				WAIT_MS
			);

			auto* model = navigation.tree->model();
			{
				QAbstractItemModelTester tester(model, QAbstractItemModelTester::FailureReportingMode::QtTest);
			}
			shown << modeName;
			if (model->rowCount() == 0)
			{
				QVERIFY2(!modesWithBooks.contains(modeName), qPrintable(modeName + ": the mode lists nothing"));
				continue;
			}

			auto item = model->index(0, 0);
			while (model->hasChildren(item))
				item = model->index(0, 0, item);

			const auto* previousBooks = books.tree->model();
			navigation.tree->setCurrentIndex(item);
			if (modesWithBooks.contains(modeName))
				QTRY_VERIFY2_WITH_TIMEOUT(
					books.tree->model() && books.tree->model() != previousBooks && books.tree->model()->rowCount() > 0,
					qPrintable(QString("%1, '%2': no books listed\n%3").arg(modeName, item.data().toString(), DescribeModel(books.tree->model()))),
					WAIT_MS
				);
			else
				QTest::qWait(300); // the books query runs, whatever it finds
		}

		for (const auto& modeName : modesWithBooks)
			QVERIFY2(shown.contains(modeName), qPrintable(QString("the test library must offer the %1 mode; offered: %2").arg(modeName, shown.join(", "))));
	}

	// An INPX author "Nikolaev [nick]" is the author Nikolaev with the nickname "nick" (#1337)
	void authorNicknameIsNotPartOfTheName()
	{
		ActivateCollection(BOOKS_COLLECTION);
		Start();

		const auto navigation = FindPanel("Navigation");
		ShowAuthors(navigation);
		if (QTest::currentTestFailed())
			return;

		const auto* model = navigation.tree->model();
		QVERIFY2(!ContainsText(*model, QString("[%1]").arg(NICKNAME)), qPrintable(DescribeModel(model)));
		QVERIFY2(CountRows(*model, AUTHOR_NIKOLAEV) == 1, qPrintable(DescribeModel(model)));
		QCOMPARE(QueryString("select NickName from Authors where LastName = ?", "Nikolaev"), QString(NICKNAME));

		ExpectBooks(navigation, FindText(*model, AUTHOR_NIKOLAEV), { PAPER_BOOK, EPUB_BOOK }, { PETROV_BOOK });
	}

	// A read book gets a coloured mark at the left of its row and a tinted background (#1430). With alternating row colours
	// the style paints the background of every other row, which once covered both there (#1321, e0d8749a)
	void readBooksAreMarkedOnEveryRow()
	{
		ActivateCollection(BOOKS_COLLECTION);
		SetSetting(Constant::Settings::PREFER_ALTERNATING_ROW_COLORS, true);
		SetSetting(READ_MARK_COLOR_KEY, READ_MARK_COLOR);
		SetSetting(READ_MARK_BACKGROUND_KEY, READ_MARK_BACKGROUND);
		SetSetting(READ_MARK_WIDTH_KEY, READ_MARK_WIDTH);
		SetSetting(READ_MARK_POSITION_KEY, 0);
		const QStringList read { PETROV_BOOK, PETROV_BOOK_2, OTHER_AUTHOR_BOOK, PAPER_BOOK, EPUB_BOOK }; // every book but the Brown one
		SetRead(read, true);
		ShowHiddenModes();
		if (QTest::currentTestFailed())
			return;
		Start();

		const auto navigation = FindPanel("Navigation");
		const auto books      = FindPanel("Books");
		ShowViewMode(books, ListViewMode);
		ShowSingleItemMode(navigation, "AllBooks");
		if (QTest::currentTestFailed())
			return;
		ExpectBooks(navigation, navigation.tree->model()->index(0, 0), read + QStringList { BROWN_BOOK }, {});
		if (QTest::currentTestFailed())
			return;

		// The mark is a vertical line centred at its position plus half its width. The background is checked at the right
		// end of the row, where no text is: an unread row there has the plain base colour, or the alternate one
		books.tree->selectionModel()->clear();
		const auto  image     = books.tree->viewport()->grab().toImage();
		const auto  blankX    = image.width() - 2;
		const auto  palette   = books.tree->palette();
		const auto  plainRows = std::set { palette.color(QPalette::Base).rgb(), palette.color(QPalette::AlternateBase).rgb() };
		const auto* model     = books.tree->model();
		QStringList actual, expected;
		for (int row = 0, rows = model->rowCount(); row < rows; ++row)
		{
			const auto index = model->index(row, 0);
			const auto y     = books.tree->visualRect(index).center().y();
			QVERIFY2(image.rect().contains(QPoint(blankX, y)), "every book row must be visible");

			const auto title  = index.data(Role::Title).toString();
			const auto prefix = QString("row %1%2 '%3': ").arg(row).arg(row % 2 ? " (alternate)" : "").arg(title);
			const auto marked = image.pixel(READ_MARK_WIDTH / 2, y) == QColor(Qt::magenta).rgb();
			const auto tinted = !plainRows.contains(image.pixel(blankX, y));
			actual << prefix + (marked ? "marked" : "not marked") + (tinted ? ", tinted" : ", plain");
			expected << prefix + (read.contains(title) ? "marked, tinted" : "not marked, plain");
		}
		SetRead(read, false);
		QCOMPARE(actual.join('\n'), expected.join('\n'));
	}

	// Hiding a books column from the header menu, opened right of the last column (a click there crashed in #1304), keeps it
	// hidden after a restart of the app (#1317)
	void hiddenBooksColumnStaysHiddenAfterRestart()
	{
		ActivateCollection(BOOKS_COLLECTION);
		Start();

		auto navigation = FindPanel("Navigation");
		auto books      = FindPanel("Books");
		ShowViewMode(books, ListViewMode);
		ShowAuthors(navigation);
		ExpectBooks(navigation, FindText(*navigation.tree->model(), AUTHOR_PETROV), { PETROV_BOOK }, {});
		if (QTest::currentTestFailed())
			return;

		const auto column = FindColumn(*books.tree->header(), BOOKS_COLUMN);
		QVERIFY2(column >= 0, qPrintable(QString("the books view must have a %1 column").arg(BOOKS_COLUMN)));
		const auto title = books.tree->header()->model()->headerData(column, Qt::Horizontal, Role::HeaderTitle).toString();
		QVERIFY2(!books.tree->header()->isSectionHidden(column), "the column must be visible at first");
		QVERIFY2(SetColumnVisible(*books.tree->header(), title, false), "the header menu must list the column");
		QVERIFY(books.tree->header()->isSectionHidden(column));

		Stop();
		Start();
		navigation = FindPanel("Navigation");
		books      = FindPanel("Books");
		ShowAuthors(navigation);
		ExpectBooks(navigation, FindText(*navigation.tree->model(), AUTHOR_PETROV), { PETROV_BOOK }, {});
		if (QTest::currentTestFailed())
			return;

		const auto hiddenAfterRestart = QTest::qWaitFor(
			[&] {
				return books.tree->header()->isSectionHidden(FindColumn(*books.tree->header(), BOOKS_COLUMN));
			},
			WAIT_MS
		);
		QVERIFY2(hiddenAfterRestart, "the hidden column must stay hidden after a restart");
		QVERIFY2(SetColumnVisible(*books.tree->header(), title, true), "the header menu must show the column again");
	}

	// With Preferences/CommonBooksTableColumnSettings on, every navigation mode shares one books column layout (#1324)
	void commonColumnSettingsApplyToEveryMode()
	{
		ActivateCollection(BOOKS_COLLECTION);
		SetSetting(COMMON_COLUMN_SETTINGS_KEY, true);
		Start();

		const auto navigation = FindPanel("Navigation");
		const auto books      = FindPanel("Books");
		ShowViewMode(books, ListViewMode);
		ShowAuthors(navigation);
		ExpectBooks(navigation, FindText(*navigation.tree->model(), AUTHOR_PETROV), { PETROV_BOOK }, {});
		if (QTest::currentTestFailed())
			return;

		auto*      header = books.tree->header();
		const auto column = FindColumn(*header, BOOKS_COLUMN);
		QVERIFY(column >= 0);
		const auto title = header->model()->headerData(column, Qt::Horizontal, Role::HeaderTitle).toString();
		QVERIFY2(SetColumnVisible(*header, title, false), "the header menu must list the column");

		ShowMode(navigation, "Series", SERIES_SAGA);
		ExpectBooks(navigation, FindText(*navigation.tree->model(), SERIES_SAGA), { PETROV_BOOK }, {});
		if (QTest::currentTestFailed())
			return;
		const auto hiddenInSeries = QTest::qWaitFor(
			[&] {
				return header->isSectionHidden(FindColumn(*header, BOOKS_COLUMN));
			},
			WAIT_MS
		);
		if (hiddenInSeries)
			QVERIFY2(SetColumnVisible(*header, title, true), "the header menu must show the column again");

		ShowAuthors(navigation);
		ExpectBooks(navigation, FindText(*navigation.tree->model(), AUTHOR_PETROV), { PETROV_BOOK }, {});
		if (QTest::currentTestFailed())
			return;
		const auto shownInAuthors = QTest::qWaitFor(
			[&] {
				return !header->isSectionHidden(FindColumn(*header, BOOKS_COLUMN));
			},
			WAIT_MS
		);
		QCOMPARE(QString("hidden in Series: %1, shown again in Authors: %2").arg(hiddenInSeries ? "yes" : "no", shownInAuthors ? "yes" : "no"), QString("hidden in Series: yes, shown again in Authors: yes"));
	}

	// Checking one of several selected books checks the whole selection; checking a book outside the selection checks only that
	// book (#1370). The test toggles the check state through the model, as a click on the check box does
	void checkingBooksFollowsTheSelection()
	{
		ActivateCollection(BOOKS_COLLECTION);
		Start();

		const auto navigation = FindPanel("Navigation");
		const auto books      = FindPanel("Books");
		ShowViewMode(books, ListViewMode);
		ShowAuthors(navigation);
		ExpectBooks(navigation, FindText(*navigation.tree->model(), AUTHOR_NIKOLAEV), { PAPER_BOOK, EPUB_BOOK }, {});
		if (QTest::currentTestFailed())
			return;

		auto*       model  = books.tree->model();
		const auto  paper  = CheckIndex(*model, PAPER_BOOK);
		const auto  epub   = CheckIndex(*model, EPUB_BOOK);
		const auto  titles = QStringList { PAPER_BOOK, EPUB_BOOK };
		QStringList actual;
		QVERIFY2(paper.isValid() && epub.isValid(), qPrintable(DescribeModel(model)));

		Select(books, { paper, epub });
		QVERIFY(model->setData(paper, Qt::Checked, Qt::CheckStateRole));
		actual << "both selected, one checked: " + DescribeChecks(*model, titles);
		QVERIFY(model->setData(paper, Qt::Unchecked, Qt::CheckStateRole));
		actual << "both selected, one unchecked: " + DescribeChecks(*model, titles);

		Select(books, { paper });
		QVERIFY(model->setData(epub, Qt::Checked, Qt::CheckStateRole));
		actual << "other one selected, one checked: " + DescribeChecks(*model, titles);
		QVERIFY(model->setData(epub, Qt::Unchecked, Qt::CheckStateRole));

		QCOMPARE(
			actual.join('\n'),
			QString(
				"both selected, one checked: Paper Pages checked, Electronic Pages checked\n"
				"both selected, one unchecked: Paper Pages unchecked, Electronic Pages unchecked\n"
				"other one selected, one checked: Paper Pages unchecked, Electronic Pages checked"
			)
		);
	}

	// In the tree view, checking a series checks the books of that series and no others (#1398)
	void checkingASeriesInTreeModeChecksOnlyItsBooks()
	{
		ActivateCollection(BOOKS_COLLECTION);
		Start();

		const auto navigation = FindPanel("Navigation");
		const auto books      = FindPanel("Books");
		ShowViewMode(books, TreeViewMode);
		ShowAuthors(navigation);
		ExpectBooks(navigation, FindText(*navigation.tree->model(), AUTHOR_NIKOLAEV), { PAPER_BOOK, EPUB_BOOK }, {});
		if (QTest::currentTestFailed())
			return;
		QTRY_VERIFY_WITH_TIMEOUT(books.tree->model()->data({}, Role::IsTree).toBool(), WAIT_MS);

		auto*      model  = books.tree->model();
		const auto series = CheckIndex(*model, SERIES_PAGES);
		QVERIFY2(series.isValid(), qPrintable(DescribeModel(model)));
		books.tree->setCurrentIndex(series.siblingAtColumn(0));
		QVERIFY(model->setData(series, Qt::Checked, Qt::CheckStateRole));
		const auto checks = DescribeChecks(*model, { EPUB_BOOK, PAPER_BOOK });
		QVERIFY(model->setData(series, Qt::Unchecked, Qt::CheckStateRole));
		ShowViewMode(books, ListViewMode);

		QCOMPARE(checks, QString("Electronic Pages checked, Paper Pages unchecked"));
	}

	// The annotation panel shows an fb2 book's annotation, its publisher details and its cover
	void fb2AnnotationShowsDescriptionPublisherAndCover()
	{
		ActivateCollection(BOOKS_COLLECTION);
		Start();

		const auto navigation = FindPanel("Navigation");
		const auto books      = FindPanel("Books");
		ShowAuthors(navigation);
		ExpectBooks(navigation, FindText(*navigation.tree->model(), AUTHOR_PETROV), { PETROV_BOOK }, {});
		if (QTest::currentTestFailed())
			return;

		books.tree->setCurrentIndex(FindText(*books.tree->model(), PETROV_BOOK));
		QTRY_VERIFY2_WITH_TIMEOUT(AnnotationText().contains(FB2_ANNOTATION) && AnnotationText().contains(FB2_PUBLISHER), qPrintable(AnnotationText()), WAIT_MS);
		QTRY_VERIFY2_WITH_TIMEOUT(HasCover(), "the book's cover must be shown", WAIT_MS);
	}

	// The annotation panel shows an ePub book's description and publisher (#1392)
	void ePubAnnotationShowsDescriptionAndPublisher()
	{
		ActivateCollection(BOOKS_COLLECTION);
		Start();

		const auto navigation = FindPanel("Navigation");
		const auto books      = FindPanel("Books");
		ShowAuthors(navigation);
		ExpectBooks(navigation, FindText(*navigation.tree->model(), AUTHOR_NIKOLAEV), { EPUB_BOOK }, {});
		if (QTest::currentTestFailed())
			return;

		books.tree->setCurrentIndex(FindText(*books.tree->model(), EPUB_BOOK));
		QTRY_VERIFY2_WITH_TIMEOUT(AnnotationText().contains(EPUB_ANNOTATION) && AnnotationText().contains(EPUB_PUBLISHER), qPrintable(AnnotationText()), WAIT_MS);
	}

	// Export as the original format and as a zip archive writes the book in the encoding chosen in the settings (#1411)
	void exportWritesTheChosenEncoding()
	{
		ActivateCollection(BOOKS_COLLECTION);
		SetSetting(EXPORT_ENCODING_KEY, EXPORT_ENCODING);
		const QTemporaryDir exportFolder(QDir(m_root).filePath("export-XXXXXX"));
		QVERIFY(exportFolder.isValid());
		ModalDialogCloser::AnswerFolderDialog(exportFolder.path());
		Start();

		const auto navigation = FindPanel("Navigation");
		const auto books      = FindPanel("Books");
		ShowViewMode(books, ListViewMode);
		ShowAuthors(navigation);
		ExpectBooks(navigation, FindText(*navigation.tree->model(), AUTHOR_PETROV), { PETROV_BOOK }, {});
		if (QTest::currentTestFailed())
			return;
		const auto book = FindText(*books.tree->model(), PETROV_BOOK);
		books.tree->setCurrentIndex(book);

		Export(books, book, EXPORT_AS_IS_KEY);
		if (QTest::currentTestFailed())
			return;
		const auto fb2 = FindFile(exportFolder.path(), "*.fb2");
		QVERIFY2(!fb2.isEmpty(), "export as the original format must write an fb2 file");
		QFile file(fb2);
		QVERIFY(file.open(QIODevice::ReadOnly));
		const auto bytes = file.readAll();
		const auto head  = bytes.left(bytes.indexOf('\n'));
		QVERIFY2(head.contains(QString(R"(encoding="%1")").arg(EXPORT_ENCODING).toUtf8()), head.constData());
		QVERIFY2(bytes.contains(CP1251_TEST), "the text must be converted to the chosen encoding");

		Export(books, book, EXPORT_AS_ZIP_KEY);
		if (QTest::currentTestFailed())
			return;
		const auto zip = FindFile(exportFolder.path(), "*.zip");
		QVERIFY2(!zip.isEmpty(), "export as a zip archive must write a zip file");
		const auto python = QStandardPaths::findExecutable("python3");
		QCOMPARE(
			QProcess::execute(
				python,
				{ "-I",
		          "-c",
		          "import sys, zipfile; z = zipfile.ZipFile(sys.argv[1]); names = [n for n in z.namelist() if n.endswith('.fb2')]; "
		          "sys.exit(0 if names and sys.argv[2].encode() in z.read(names[0]).split(b'\\n')[0] else 1)",
		          zip,
		          QString(R"(encoding="%1")").arg(EXPORT_ENCODING) }
			),
			0
		);
	}

	// A user data backup holds groups, ratings and searches; restoring it brings them back after they were deleted
	void userDataSurvivesBackupAndRestore()
	{
		ActivateCollection(BOOKS_COLLECTION);
		long long authorId = 0;
		AddGroupWithAuthor(BACKUP_GROUP, "Smith", authorId);
		SetRead({ BROWN_BOOK }, true);
		ExecuteSql("insert into Searches_User(Title, CreatedAt) values(?, datetime('now'))", BACKUP_SEARCH);
		if (QTest::currentTestFailed())
			return;

		const auto stored = UserDataState(authorId);
		QCOMPARE(stored, QString("group with Smith: 1, 'Lost Letters' rated: 1, search: 1"));

		const auto path = QDir(m_root).filePath(QString("user-data") + Constant::BACKUP_FILE_EXT);
		QVERIFY2(RunUserDataController(&IUserDataController::Backup, path), "the backup must succeed");

		ExecuteSql("delete from Groups_List_User where GroupID in (select GroupID from Groups_User where Title = ?)", BACKUP_GROUP);
		ExecuteSql("delete from Groups_User where Title = ?", BACKUP_GROUP);
		ExecuteSql("delete from Searches_User where Title = ?", BACKUP_SEARCH);
		SetRead({ BROWN_BOOK }, false);
		QCOMPARE(UserDataState(authorId), QString("group with Smith: 0, 'Lost Letters' rated: 0, search: 0"));

		QVERIFY2(RunUserDataController(&IUserDataController::Restore, path), "the restore must succeed");
		const auto restored = UserDataState(authorId);

		ExecuteSql("delete from Groups_List_User where GroupID in (select GroupID from Groups_User where Title = ?)", BACKUP_GROUP);
		ExecuteSql("delete from Groups_User where Title = ?", BACKUP_GROUP);
		ExecuteSql("delete from Searches_User where Title = ?", BACKUP_SEARCH);
		SetRead({ BROWN_BOOK }, false);
		QCOMPARE(restored, stored);
	}

	// Updating a collection from a newer INPX adds its books, keeps an author with a [nickname] one author, and keeps the
	// user's groups and ratings (3d4594ac: the update mode once lost the nickname handling)
	void collectionUpdateAddsBooksAndKeepsUserData()
	{
		ActivateCollection(UPDATE_COLLECTION);
		long long authorId = 0;
		AddGroupWithAuthor(GROUP_TITLE, "Brown", authorId);
		SetRead({ OTHER_AUTHOR_BOOK }, true);
		if (QTest::currentTestFailed())
			return;

		const auto python = QStandardPaths::findExecutable("python3");
		QCOMPARE(QProcess::execute(python, { "-I", FLIBRARY_TEST_FIXTURE_SCRIPT, ArchivesFolder(UPDATE_COLLECTION), "--update" }), 0);

		// The app finds the changed INPX at start-up and asks whether to update; the dialog closer gives the default answer, yes
		Start();
		QTRY_VERIFY2_WITH_TIMEOUT(QueryString("select count(42) from Books where Title = ?", UPDATE_BOOK) == "1", "the collection update must add the new book", WAIT_MS);
		Stop();
		Start();

		const auto navigation = FindPanel("Navigation");
		ShowAuthors(navigation);
		if (QTest::currentTestFailed())
			return;

		QCOMPARE(CountRows(*navigation.tree->model(), AUTHOR_NIKOLAEV), 1);
		ExpectBooks(navigation, FindText(*navigation.tree->model(), AUTHOR_NIKOLAEV), { PAPER_BOOK, EPUB_BOOK, UPDATE_BOOK }, { PETROV_BOOK });
		QCOMPARE(
			QString("in the group: %1, rated: %2")
				.arg(IsInGroup(GROUP_TITLE, authorId) ? "yes" : "no", QueryString("select UserRate from Books_View where Title = ?", OTHER_AUTHOR_BOOK).isEmpty() ? "no" : "yes"),
			QString("in the group: yes, rated: yes")
		);
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

	// The keys of a menu's items, for failure messages
	static QString DescribeMenu(const IDataItem::Ptr& menu)
	{
		QStringList keys;
		for (size_t i = 0, sz = menu->GetChildCount(); i < sz; ++i)
			keys << "'" + menu->GetChild(i)->GetData(MenuItem::Column::Key) + "'";
		return keys.join(", ");
	}

	// The child menu item with the given key, or null
	static IDataItem::Ptr FindMenuChild(const IDataItem::Ptr& menu, const QString& key)
	{
		for (size_t i = 0, sz = menu->GetChildCount(); i < sz; ++i)
			if (auto child = menu->GetChild(i); child->GetData(MenuItem::Column::Key) == key)
				return child;
		return {};
	}

	// Modes like History and All books have one navigation item without text; waits for that item, not the previous mode's list
	static void ShowSingleItemMode(const Panel& navigation, const char* modeName)
	{
		QVERIFY(navigation.tree && navigation.modeButton);
		auto* action = FindModeAction(*navigation.modeButton, modeName);
		QVERIFY2(action, qPrintable(QString("the app must offer the %1 mode").arg(modeName)));
		action->trigger();
		QTRY_VERIFY2_WITH_TIMEOUT(
			navigation.tree->model() && navigation.tree->model()->rowCount() == 1 && navigation.tree->model()->index(0, 0).data().toString().isEmpty(),
			qPrintable(DescribeModel(navigation.tree->model())),
			WAIT_MS
		);
	}

	// The books view mode (list or tree) is a global setting too
	static void ShowViewMode(const Panel& books, const char* modeName)
	{
		QVERIFY(books.tree && books.modeButton);
		auto* action = FindModeAction(*books.modeButton, modeName);
		QVERIFY2(action, qPrintable(QString("the books view must offer the %1 mode").arg(modeName)));
		if (!action->isChecked())
			action->trigger();
		QTRY_VERIFY_WITH_TIMEOUT(action->isChecked(), WAIT_MS);
	}

	// Top-level rows whose first column is exactly the text
	static int CountRows(const QAbstractItemModel& model, const QString& text)
	{
		int count = 0;
		for (int row = 0, rows = model.rowCount(); row < rows; ++row)
			if (model.index(row, 0).data().toString() == text)
				++count;
		return count;
	}

	// The check box cell of the book or tree node whose first column is exactly the title
	static QModelIndex CheckIndex(const QAbstractItemModel& model, const QString& title)
	{
		const auto found = model.match(model.index(0, 0), Qt::DisplayRole, title, 1, Qt::MatchExactly | Qt::MatchRecursive);
		if (found.isEmpty())
			return {};
		const auto column = found.front().data(Role::CheckableColumn);
		return column.isValid() ? found.front().siblingAtColumn(column.toInt()) : QModelIndex {};
	}

	static QString DescribeChecks(const QAbstractItemModel& model, const QStringList& titles)
	{
		QStringList result;
		for (const auto& title : titles)
		{
			const auto state = CheckIndex(model, title).data(Qt::CheckStateRole);
			result << title + (!state.isValid() ? " has no check box" : state.value<Qt::CheckState>() == Qt::Checked ? " checked" : " unchecked");
		}
		return result.join(", ");
	}

	// Selects whole rows, the first one current, as clicks with Ctrl do
	static void Select(const Panel& books, const QModelIndexList& indices)
	{
		auto* selectionModel = books.tree->selectionModel();
		selectionModel->setCurrentIndex(indices.front(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		for (const auto& index : indices)
			selectionModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
	}

	// The logical index of the books column with the given name, or -1
	static int FindColumn(const QHeaderView& header, const QString& name)
	{
		for (int i = 0, sz = header.count(); i < sz; ++i)
			if (header.model()->headerData(i, Qt::Horizontal, Role::HeaderName).toString().compare(name, Qt::CaseInsensitive) == 0)
				return i;
		return -1;
	}

	// Opens the books header menu with a right click past the last column and sets the column's check box in it, as a user does
	static bool SetColumnVisible(QHeaderView& header, const QString& columnTitle, const bool visible)
	{
		bool   changed = false;
		QTimer menuWatcher;
		QObject::connect(&menuWatcher, &QTimer::timeout, [&] {
			auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
			if (!menu)
				return;
			menuWatcher.stop();
			if (auto* view = menu->findChild<QAbstractItemView*>())
			{
				auto*      model = view->model();
				const auto found = model->match(model->index(0, 0), Qt::DisplayRole, columnTitle, 1, Qt::MatchExactly);
				if (!found.isEmpty() && (found.front().data(Qt::CheckStateRole).value<Qt::CheckState>() == Qt::Checked) != visible)
					changed = model->setData(found.front(), visible ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
			}
			menu->close();
		});
		menuWatcher.start(std::chrono::milliseconds(50));
		emit header.customContextMenuRequested(QPoint(header.length() + 10, header.height() / 2));
		return changed;
	}

	// Chooses an Export item in the book's context menu and waits until the export finishes without an error
	void Export(const Panel& books, const QModelIndex& book, const QString& key) const
	{
		const auto controller = m_container->resolve<ILogicFactory>()->GetTreeViewController(ItemType::Books);
		const auto menu       = std::make_shared<IDataItem::Ptr>();
		controller->RequestContextMenu(book, ITreeViewController::RequestContextMenuOptions::HasSelection, [menu](const QString&, const IDataItem::Ptr& item) {
			*menu = item;
		});
		QTRY_VERIFY_WITH_TIMEOUT(*menu, WAIT_MS);

		const auto exportMenu = FindMenuChild(*menu, EXPORT_KEY);
		QVERIFY2(exportMenu, qPrintable("the book menu must offer Export; it has " + DescribeMenu(*menu)));
		const auto item = FindMenuChild(exportMenu, key);
		QVERIFY2(item, qPrintable(QString("the Export menu must offer %1; it has %2").arg(key, DescribeMenu(exportMenu))));

		// The export marks the menu item with its result when it is done
		controller->OnContextMenuTriggered(books.tree->model(), book, { book }, item);
		QTRY_VERIFY2_WITH_TIMEOUT(!item->GetData(MenuItem::Column::HasError).isEmpty(), qPrintable(QString("%1 did not finish").arg(key)), WAIT_MS);
		QCOMPARE(item->GetData(MenuItem::Column::HasError), QString("0"));
	}

	// The first file below the folder that matches the pattern
	static QString FindFile(const QString& folder, const QString& pattern)
	{
		QDirIterator it(folder, { pattern }, QDir::Files, QDirIterator::Subdirectories);
		return it.hasNext() ? it.next() : QString {};
	}

	// Everything the annotation panel shows as text
	static QString AnnotationText()
	{
		QStringList result;
		for (auto* topLevel : QApplication::topLevelWidgets())
			for (const auto* label : topLevel->findChildren<QLabel*>("info"))
				result << label->text();
		return result.join('\n');
	}

	static bool HasCover()
	{
		for (auto* topLevel : QApplication::topLevelWidgets())
			for (const auto* label : topLevel->findChildren<QLabel*>("cover"))
				if (!label->pixmap().isNull())
					return true;
		return false;
	}

	// Chooses a navigation mode from the main window's Navigation menu. The mode button would do, but History, All books and
	// Already read hide the navigation panel with it, and the menu is how a user leaves them
	static void TriggerNavigationMenu(const QString& modeName)
	{
		for (auto* topLevel : QApplication::topLevelWidgets())
			if (auto* action = topLevel->findChild<QAction*>(modeName + "_isActive"))
			{
				action->trigger();
				return;
			}
		QFAIL(qPrintable(QString("the Navigation menu must offer %1").arg(modeName)));
	}

	// Offers the All books and Already read modes, which the app hides until the user switches them on
	void ShowHiddenModes()
	{
		SetSetting(QString(MODE_HIDDEN_KEY).arg("AllBooks"), false);
		SetSetting(QString(MODE_HIDDEN_KEY).arg("AlreadyRead"), false);
	}

	// Stores a setting for this test only: cleanup() removes it
	void SetSetting(const QString& key, const QVariant& value)
	{
		std::shared_ptr<Hypodermic::Container> container;
		BuildContainer(container);
		container->resolve<ISettings>()->Set(key, value);
		m_changedSettings << key;
	}

	// Runs one statement with a text value on the active collection
	static void ExecuteSql(const char* sql, const QString& value)
	{
		std::shared_ptr<Hypodermic::Container> container;
		BuildContainer(container);
		const auto transaction = container->resolve<IDatabaseController>()->GetDatabase()->CreateTransaction();
		QVERIFY(transaction->CreateCommand(sql)->Execute(value.toStdString()));
		QVERIFY(transaction->Commit());
	}

	// Rates the books, which marks them read, or removes their rating
	static void SetRead(const QStringList& titles, const bool read)
	{
		for (const auto& title : titles)
			ExecuteSql(
				read ? "insert or replace into Books_User(BookID, UserRate, CreatedAt) select BookID, 5, datetime('now') from Books where Title = ?"
					 : "update Books_User set UserRate = null where BookID in (select BookID from Books where Title = ?)",
				title
			);
	}

	// The first column of the first row the query finds in the active collection, or an empty string
	static QString QueryString(const std::string& sql, const QString& value)
	{
		std::shared_ptr<Hypodermic::Container> container;
		BuildContainer(container);
		const auto query = container->resolve<IDatabaseController>()->GetDatabase()->CreateQuery(sql);
		query->Bind(0, value.toStdString());
		query->Execute();
		return query->Eof() || query->IsNull(0) ? QString {} : query->GetQString(0);
	}

	// What the backup test stores, as counts
	static QString UserDataState(const long long authorId)
	{
		const auto group  = QueryString("select count(42) from Groups_List_User l join Groups_User g on g.GroupID = l.GroupID where g.Title = ? and l.ObjectID = " + std::to_string(authorId), BACKUP_GROUP);
		const auto rating = QueryString("select count(42) from Books_View where Title = ? and UserRate is not null", BROWN_BOOK);
		const auto search = QueryString("select count(42) from Searches_User where Title = ?", BACKUP_SEARCH);
		return QString("group with Smith: %1, 'Lost Letters' rated: %2, search: %3").arg(group, rating, search);
	}

	// Runs a backup or a restore of the active collection's user data and waits for its result
	static bool RunUserDataController(void (IUserDataController::*action)(QString, IUserDataController::Callback) const, const QString& path)
	{
		std::shared_ptr<Hypodermic::Container> container;
		BuildContainer(container);
		const auto controller = container->resolve<ILogicFactory>()->CreateUserDataController();

		const auto result = std::make_shared<std::optional<bool>>();
		((*controller).*action)(path, [result](const bool ok, const QString& message) {
			qInfo("user data: %s", qPrintable(message));
			*result = ok;
		});
		return QTest::qWaitFor(
				   [&] {
					   return result->has_value();
				   },
				   WAIT_MS
			   )
		    && **result;
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

		// The app's event loop runs the deferred start-up calls first. Destroying the window before they ran trips an assert in
		// the Debug build: TreeView unregisters from the menu customizer, which its deferred call has not yet registered with
		QTest::qWait(0);
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
	QStringList                            m_changedSettings;
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
	// On a GNOME or KDE desktop Qt loads the desktop's platform theme even offscreen, and its native file dialogs would open on
	// the real screen, out of the dialog closer's reach
	QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);

	QApplication app(argc, argv);
	// ICU, which converts text between encodings, finds its data file beside the libraries, as run.sh and start.sh tell it
	if (qEnvironmentVariableIsEmpty("ICU_DATA"))
		qputenv("ICU_DATA", QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../lib").toUtf8());
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
