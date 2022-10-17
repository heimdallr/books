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
		m_booksModelController->RegisterObserver(this);
	}

	~Impl() override
	{
		m_authorsModelController->UnregisterObserver(this);
		m_booksModelController->UnregisterObserver(this);
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
			auto * authorsModel = controller->GetModel();
			int localIndex = index;
			authorsModel->setData({}, QVariant::fromValue(TranslateIndexFromGlobalRequest{ &localIndex }), RoleBase::TranslateIndexFromGlobal);
			const auto localModelIndex = authorsModel->index(localIndex, 0);
			assert(localModelIndex.isValid());
			const auto authorIdVar = authorsModel->data(localModelIndex, RoleBase::Id);
			assert(authorIdVar.isValid());
			m_booksModelController->SetAuthorId(authorIdVar.toInt());
		}
	}

	void OnKeyPressed(const int key, const int modifiers) override
	{
		if (key == 45 && modifiers == Qt::AltModifier)
			Util::Set(m_running, false, m_self, &GuiController::RunningChanged);
	}

private:
	GuiController & m_self;
	QQmlApplicationEngine m_qmlEngine;
	PropagateConstPtr<Util::Executor> m_executor{Util::ExecutorFactory::Create(Util::ExecutorImpl::Async) };
	PropagateConstPtr<DB::Database> m_db;
	PropagateConstPtr<AuthorsModelController> m_authorsModelController { std::make_unique<AuthorsModelController>(*m_executor, *m_db) };
	PropagateConstPtr<BooksModelController> m_booksModelController { std::make_unique<BooksModelController>(*m_executor, *m_db) };
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
