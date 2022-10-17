#include <QAbstractItemModel>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "fnd/algorithm.h"
#include "fnd/executor.h"
#include "fnd/executor/factory.h"

#include "database/factory/Factory.h"

#include "database/interface/Database.h"

#include "models/RoleBase.h"

#include "GuiController.h"

#include "ModelControllers/ModelController.h"
#include "ModelControllers/ModelControllerObserver.h"
#include "ModelControllers/AuthorsModelController.h"
#include "ModelControllers/BooksModelController.h"

namespace HomeCompa::Flibrary {

class GuiController::Impl
	: virtual public ModelControllerObserver
{
	NON_COPY_MOVABLE(Impl)
public:
	Impl(GuiController & self, const std::string & databaseName)
		: m_self(self)
		, m_db(Create(DB::Factory::Impl::Sqlite, databaseName))
	{
		m_authorsModelController->RegisterObserver(this);
	}

	~Impl() override
	{
		m_qmlEngine.clearComponentCache();
	}

	void Start()
	{
		auto * const qmlContext = m_qmlEngine.rootContext();
		qmlContext->setContextProperty("guiController", &m_self);
		qRegisterMetaType<QAbstractItemModel *>("QAbstractItemModel*");
		qRegisterMetaType<ModelController *>("ModelController*");
		m_qmlEngine.load("qrc:/Main.qml");
	}

	bool GetRunning() const noexcept
	{
		return m_running;
	}

	void OnKeyPressed(const int key, const int modifiers)
	{
		if (key == Qt::Key_X && modifiers == Qt::AltModifier)
			Util::Set(m_running, false, m_self, &GuiController::RunningChanged);

		m_focusedController->OnKeyPressed(key, modifiers);
	}

	ModelController * GetAuthorsModelController() noexcept
	{
		return m_authorsModelController.get();
	}

	ModelController * GetBooksModelController() noexcept
	{
		return m_booksModelController.get();
	}

private: // ModelControllerObserver
	void HandleCurrentIndexChanged(ModelController * const controller, const int index) override
	{
		if (controller->GetType() == ModelController::Type::Authors)
		{
			const auto * authorsModel = controller->GetModel();
			const auto authorIdVar = authorsModel->data(authorsModel->index(index, 0), RoleBase::Id);
			assert(authorIdVar.isValid());
			m_booksModelController->SetAuthorId(authorIdVar.toInt());
		}
	}

	void HandleClicked(ModelController * const controller, const int /*index*/) override
	{
		m_focusedController = controller;
		emit m_self.MainWindowFocusedChanged();
	}

private:
	GuiController & m_self;
	QQmlApplicationEngine m_qmlEngine;
	PropagateConstPtr<Executor> m_executor{ ExecutorFactory::Create(ExecutorImpl::Sync) };
	PropagateConstPtr<DB::Database> m_db;
	PropagateConstPtr<AuthorsModelController> m_authorsModelController { std::make_unique<AuthorsModelController>(*m_executor, *m_db) };
	PropagateConstPtr<BooksModelController> m_booksModelController { std::make_unique<BooksModelController>(*m_executor, *m_db) };
	ModelController * m_focusedController { m_authorsModelController.get() };
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

void GuiController::OnKeyPressed(const int key, const int modifiers)
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
