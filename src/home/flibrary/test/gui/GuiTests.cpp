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

#include "interface/localization.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/IDatabaseMigrator.h"
#include "interface/logic/ITaskQueue.h"
#include "interface/ui/IMainWindow.h"

#include "Hypodermic/Hypodermic.h"
#include "djvu/djvu.h"
#include "genre/Genre.h"
#include "gui/di_ui.h"
#include "gutil/di_gutil.h"
#include "logging/init.h"
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
constexpr auto AUTHOR_PETROV     = "Petrov Ivan";
constexpr auto PETROV_BOOK       = "First Test Book";
constexpr auto OTHER_AUTHOR_BOOK = "Detective Story";

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

private:
	// The navigation mode is a global setting that an earlier test may have changed, so each test chooses its own
	static void ShowAuthors(const Panel& navigation)
	{
		QVERIFY(navigation.tree && navigation.modeButton);
		auto* authors = FindModeAction(*navigation.modeButton, "Authors");
		QVERIFY2(authors, "the test library must offer the Authors mode");
		authors->trigger();
		QTRY_VERIFY2_WITH_TIMEOUT(navigation.tree->model() && ContainsText(*navigation.tree->model(), AUTHOR_PETROV), qPrintable(DescribeModel(navigation.tree->model())), WAIT_MS);
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
