#include <QAbstractItemModel>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "fnd/algorithm.h"

#include "database/factory/Factory.h"

#include "database/interface/Database.h"

#include "models/RoleBase.h"

#include "util/executor.h"
#include "util/executor/factory.h"

#include "GuiController.h"

#include "ModelControllers/BooksModelController.h"
#include "ModelControllers/ModelController.h"
#include "ModelControllers/ModelControllerObserver.h"
#include "ModelControllers/NavigationModelController.h"
#include "ModelControllers/NavigationSource.h"

#include "Configuration.h"

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
		, m_executor(Util::ExecutorFactory::Create(Util::ExecutorImpl::Async, [&] { CreateDatabase(databaseName).swap(m_db); }))
	{
		PropagateConstPtr<BooksModelController>(std::make_unique<BooksModelController>(*m_executor, *m_db)).swap(m_booksModelController);
		m_booksModelController->RegisterObserver(this);
	}

	~Impl() override
	{
		m_qmlEngine.clearComponentCache();
		for (auto & [_, controller] : m_navigationModelControllers)
			controller->UnregisterObserver(this);
		m_booksModelController->UnregisterObserver(this);

	}

	void Start()
	{
		auto * const qmlContext = m_qmlEngine.rootContext();
		qmlContext->setContextProperty("guiController", &m_self);
		qRegisterMetaType<QAbstractItemModel *>("QAbstractItemModel*");
		qRegisterMetaType<ModelController *>("ModelController*");
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

	ModelController * GetNavigationModelController(const NavigationSource navigationSource)
	{
		m_navigationSource = navigationSource;
		const auto it = m_navigationModelControllers.find(navigationSource);
		if (it != m_navigationModelControllers.end())
			return it->second.get();

		auto & controller = m_navigationModelControllers.emplace(navigationSource, std::make_unique<NavigationModelController>(*m_executor, *m_db, navigationSource)).first->second;
		controller->RegisterObserver(this);

		return controller.get();
	}

	ModelController * GetBooksModelController() noexcept
	{
		return m_booksModelController.get();
	}

	bool IsFieldVisible(const NavigationSource navigationSource) const noexcept
	{
		return navigationSource != m_navigationSource;
	}

private: // ModelControllerObserver
	void HandleCurrentIndexChanged(ModelController * const controller, int index) override
	{
		if (controller->GetType() == ModelController::Type::Navigation)
			m_booksModelController->SetNavigationState(m_navigationSource, controller->GetId(index));
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
		setFocus(m_booksModelController.get());
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

ModelController * GuiController::GetBooksModelController() noexcept
{
	return m_impl->GetBooksModelController();
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

bool GuiController::GetRunning() const noexcept
{
	return m_impl->GetRunning();
}

}
