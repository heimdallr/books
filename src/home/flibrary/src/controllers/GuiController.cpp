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

#include "ModelControllers/ModelController.h"
#include "ModelControllers/ModelControllerObserver.h"
#include "ModelControllers/NavigationModelController.h"
#include "ModelControllers/BooksModelController.h"

namespace HomeCompa::Flibrary {

PropagateConstPtr<DB::Database> CreateDatabase(const std::string & databaseName)
{
	return PropagateConstPtr<DB::Database>(Create(DB::Factory::Impl::Sqlite, databaseName));
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
		PropagateConstPtr<NavigationModelController>(std::make_unique<NavigationModelController>(*m_executor, *m_db)).swap(m_navigationModelController);
		PropagateConstPtr<BooksModelController>(std::make_unique<BooksModelController>(*m_executor, *m_db)).swap(m_booksModelController);
		m_navigationModelController->RegisterObserver(this);
		m_booksModelController->RegisterObserver(this);
	}

	~Impl() override
	{
		m_qmlEngine.clearComponentCache();
		m_navigationModelController->UnregisterObserver(this);
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

	ModelController * GetAuthorsModelController() noexcept
	{
		return m_navigationModelController.get();
	}

	ModelController * GetBooksModelController() noexcept
	{
		return m_booksModelController.get();
	}

private: // ModelControllerObserver
	void HandleCurrentIndexChanged(ModelController * const controller, int index) override
	{
		if (controller->GetType() == ModelController::Type::Authors)
		{
			auto * authorsModel = controller->GetModel();
			authorsModel->setData({}, QVariant::fromValue(TranslateIndexFromGlobalRequest{ &index }), RoleBase::TranslateIndexFromGlobal);
			const auto localModelIndex = authorsModel->index(index, 0);
			assert(localModelIndex.isValid());
			const auto authorIdVar = authorsModel->data(localModelIndex, RoleBase::Id);
			assert(authorIdVar.isValid());
			m_booksModelController->SetAuthorId(authorIdVar.toInt());
		}
	}

	void HandleClicked(ModelController * controller) override
	{
		m_activeModelController = controller;

		const auto setFocus = [&] (ModelController * modelController)
		{
			modelController->SetFocused(modelController == m_activeModelController);
		};

		setFocus(m_navigationModelController.get());
		setFocus(m_booksModelController.get());
	}

private:
	GuiController & m_self;
	QQmlApplicationEngine m_qmlEngine;
	PropagateConstPtr<DB::Database> m_db;
	PropagateConstPtr<Util::Executor> m_executor;
	PropagateConstPtr<NavigationModelController> m_navigationModelController;
	PropagateConstPtr<BooksModelController> m_booksModelController;
	ModelController * m_activeModelController { nullptr };
	bool m_running { true };
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

ModelController * GuiController::GetAuthorsModelController() noexcept
{
	return m_impl->GetAuthorsModelController();
}

ModelController * GuiController::GetBooksModelController() noexcept
{
	return m_impl->GetBooksModelController();
}

bool GuiController::GetRunning() const noexcept
{
	return m_impl->GetRunning();
}

}
