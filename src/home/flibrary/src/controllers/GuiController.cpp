#pragma warning(push, 0)
#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFileDialog>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSystemTrayIcon>
#pragma warning(pop)

#include "fnd/algorithm.h"

#include "database/factory/Factory.h"

#include "database/interface/Database.h"

#include "models/RoleBase.h"

#include "util/executor.h"
#include "util/executor/factory.h"

#include "util/Settings.h"

#include "GuiController.h"

#include "ModelControllers/BooksModelController.h"
#include "ModelControllers/BooksViewType.h"
#include "ModelControllers/ModelController.h"
#include "ModelControllers/ModelControllerObserver.h"
#include "ModelControllers/NavigationModelController.h"
#include "ModelControllers/NavigationSource.h"

#include "Settings/DelegateSettings.h"
#include "Configuration.h"

Q_DECLARE_METATYPE(QSystemTrayIcon::ActivationReason)

namespace HomeCompa::Flibrary {

namespace {

PropagateConstPtr<DB::Database> CreateDatabase(const std::string & databaseName)
{
	const std::string connectionString = std::string("path=") + databaseName + ";extension=" + MHL_SQLITE_EXTENSION;
	return PropagateConstPtr<DB::Database>(Create(DB::Factory::Impl::Sqlite, connectionString));
}

}

class GuiController::Impl
	: virtual public ModelControllerObserver
{
	NON_COPY_MOVABLE(Impl)
public:
	Impl(GuiController & self, const std::string & databaseName)
		: m_self(self)
	{
		if (!databaseName.empty())
		{
			CreateExecutor(databaseName);
			return;
		}

		const auto currentDbId = m_settings.Get("database/current", {}).toString();
		if (currentDbId.isEmpty())
			return;

		const auto currentDbFileName = m_settings.Get("database/" + currentDbId + "/database", {}).toString();
		if (currentDbFileName.isEmpty())
			return;

		CreateExecutor(currentDbFileName.toStdString());
	}

	~Impl() override
	{
		m_qmlEngine.clearComponentCache();
		for (auto & [_, controller] : m_navigationModelControllers)
			controller->UnregisterObserver(this);

		if (m_booksModelController)
			m_booksModelController->UnregisterObserver(this);
	}

	void Start()
	{
		auto * const qmlContext = m_qmlEngine.rootContext();
		qmlContext->setContextProperty("guiController", &m_self);
		qmlContext->setContextProperty("iconTray", QIcon(":/icons/tray.png"));

		qmlRegisterType<QSystemTrayIcon>("QSystemTrayIcon", 1, 0, "QSystemTrayIcon");
		qRegisterMetaType<QSystemTrayIcon::ActivationReason>("ActivationReason");
		qRegisterMetaType<QAbstractItemModel *>("QAbstractItemModel*");
		qRegisterMetaType<ModelController *>("ModelController*");
		qRegisterMetaType<DelegateSettings *>("DelegateSettings*");

		m_qmlEngine.load("qrc:/Main.qml");
	}

	void OnKeyPressed(int key, int modifiers)
	{
		if (key == Qt::Key_X && modifiers == Qt::AltModifier)
			return (void)Util::Set(m_running, false, m_self, &GuiController::RunningChanged);

		if (m_activeModelController)
			m_activeModelController->OnKeyPressed(key, modifiers);
	}

	bool GetRunning() const noexcept
	{
		return m_running;
	}

	bool GetOpened() const noexcept
	{
		return !!m_db;
	}

	ModelController * GetNavigationModelController(const NavigationSource navigationSource)
	{
		if (m_navigationSource != navigationSource)
			m_currentNavigationIndex = -1;

		m_navigationSource = navigationSource;
		emit m_self.AuthorsVisibleChanged();
		emit m_self.SeriesVisibleChanged();
		emit m_self.GenresVisibleChanged();

		const auto it = m_navigationModelControllers.find(navigationSource);
		if (it != m_navigationModelControllers.end())
			return it->second.get();

		auto & controller = m_navigationModelControllers.emplace(navigationSource, std::make_unique<NavigationModelController>(*m_executor, *m_db, navigationSource)).first->second;
		QQmlEngine::setObjectOwnership(controller.get(), QQmlEngine::CppOwnership);
		controller->RegisterObserver(this);

		return controller.get();
	}

	ModelController * GetBooksModelController(const BooksViewType type) noexcept
	{
		if (m_booksViewType == type)
			return m_booksModelController.get();

		m_booksViewType = type;
		PropagateConstPtr<BooksModelController>(std::make_unique<BooksModelController>(*m_executor, *m_db, type)).swap(m_booksModelController);
		QQmlEngine::setObjectOwnership(m_booksModelController.get(), QQmlEngine::CppOwnership);
		m_booksModelController->RegisterObserver(this);

		if (m_navigationSource != NavigationSource::Undefined && m_currentNavigationIndex != -1)
			m_booksModelController->SetNavigationState(m_navigationSource, m_navigationModelControllers[m_navigationSource]->GetId(m_currentNavigationIndex));

		return m_booksModelController.get();
	}

	void AddCollection(const QString & name, const QString & db, const QString & folder)
	{
		QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
		hash.addData(db.toUtf8());
		const auto currentDb = hash.result().toHex();
		const auto dbSection = "database/" + currentDb;

		m_settings.Set(dbSection + "/name", name);
		m_settings.Set(dbSection + "/database", db);
		m_settings.Set(dbSection + "/folder", folder);

		m_settings.Set("database/current", currentDb);

		CreateExecutor(db.toStdString());
	}

	bool IsFieldVisible(const NavigationSource navigationSource) const noexcept
	{
		return navigationSource != m_navigationSource;
	}

private: // ModelControllerObserver
	void HandleCurrentIndexChanged(ModelController * const controller, const int index) override
	{
		if (controller->GetType() == ModelController::Type::Navigation)
		{
			m_currentNavigationIndex = index;
			if (m_booksModelController)
				m_booksModelController->SetNavigationState(m_navigationSource, controller->GetId(index));
		}
	}

	void HandleClicked(ModelController * controller) override
	{
		m_activeModelController = controller;

		const auto setFocus = [&] (ModelController * modelController)
		{
			modelController->SetFocused(modelController == m_activeModelController);
		};

		for (auto & [_, navigationModelController] : m_navigationModelControllers)
			setFocus(navigationModelController.get());

		if (m_booksModelController)
			setFocus(m_booksModelController.get());
	}

	void CreateExecutor(const std::string & databaseName)
	{
		auto executor = Util::ExecutorFactory::Create(Util::ExecutorImpl::Async, [databaseName, &db = m_db]
		{
			CreateDatabase(databaseName).swap(db);
		});

		PropagateConstPtr<Util::Executor>(std::move(executor)).swap(m_executor);
		emit m_self.OpenedChanged();
	}

private:
	GuiController & m_self;
	QQmlApplicationEngine m_qmlEngine;
	PropagateConstPtr<DB::Database> m_db;
	PropagateConstPtr<Util::Executor> m_executor;
	std::map<NavigationSource, PropagateConstPtr<NavigationModelController>> m_navigationModelControllers;
	PropagateConstPtr<BooksModelController> m_booksModelController;
	ModelController * m_activeModelController { nullptr };
	bool m_running { true };
	NavigationSource m_navigationSource { NavigationSource::Undefined };
	BooksViewType m_booksViewType { BooksViewType::Undefined };
	int m_currentNavigationIndex = -1;

	Settings m_settings{ "HomeCompa", "Flibrary" };
};

GuiController::GuiController(const std::string & databaseName, QObject * parent)
	: QObject(parent)
	, m_impl(*this, databaseName)
{
}

GuiController::~GuiController() = default;

void GuiController::Start()
{
	m_impl->Start();
}

void GuiController::OnKeyPressed(int key, int modifiers)
{
	m_impl->OnKeyPressed(key, modifiers);
}

ModelController * GuiController::GetNavigationModelControllerAuthors()
{
	return m_impl->GetNavigationModelController(NavigationSource::Authors);
}

ModelController * GuiController::GetNavigationModelControllerSeries()
{
	return m_impl->GetNavigationModelController(NavigationSource::Series);
}

ModelController * GuiController::GetNavigationModelControllerGenres()
{
	return m_impl->GetNavigationModelController(NavigationSource::Genres);
}

ModelController * GuiController::GetBooksModelControllerList()
{
	return m_impl->GetBooksModelController(BooksViewType::List);
}

ModelController * GuiController::GetBooksModelControllerTree()
{
	return m_impl->GetBooksModelController(BooksViewType::Tree);
}

void GuiController::AddCollection(const QString & name, const QString & db, const QString & folder)
{
	m_impl->AddCollection(name, db, folder);
}

QString GuiController::SelectFile(const QString & fileName) const
{
	return QFileDialog::getOpenFileName(nullptr, QCoreApplication::translate("FileDialog", "Select database file"), fileName);
}

QString GuiController::SelectFolder(const QString & folderName) const
{
	return QFileDialog::getExistingDirectory(nullptr, QCoreApplication::translate("FileDialog", "Select archives folder"), folderName);
}

bool GuiController::IsAuthorsVisible() const noexcept
{
	return m_impl->IsFieldVisible(NavigationSource::Authors);
}

bool GuiController::IsSeriesVisible() const noexcept
{
	return m_impl->IsFieldVisible(NavigationSource::Series);
}

bool GuiController::IsGenresVisible() const noexcept
{
	return m_impl->IsFieldVisible(NavigationSource::Genres);
}

bool GuiController::GetOpened() const noexcept
{
	return m_impl->GetOpened();
}

bool GuiController::GetRunning() const noexcept
{
	return m_impl->GetRunning();
}

}
