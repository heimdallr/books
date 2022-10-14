#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "fnd/algorithm.h"

#include "database/factory/Factory.h"

#include "database/interface/Database.h"

#include "models/AuthorsModel.h"

#include "GuiController.h"

#include "ModelController.h"
#include "ModelControllerObserver.h"

namespace HomeCompa::Flibrary {

namespace {

class AuthorsModelController : public ModelController
{
public:
	explicit AuthorsModelController(DB::Database & db)
		: m_model(std::unique_ptr<QAbstractItemModel>(CreateAuthorsModel(db)))
	{
		QQmlEngine::setObjectOwnership(m_model.get(), QQmlEngine::CppOwnership);
		QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
		SetModel(m_model.get());
	}

private:
	PropagateConstPtr<QAbstractItemModel> m_model;
};

}

class GuiController::Impl
	: virtual public ModelControllerObserver
{
	NON_COPY_MOVABLE(Impl)
public:
	Impl(GuiController & self, const std::string & databaseName)
		: m_self(self)
		, m_db(Create(DB::Factory::Impl::Sqlite, databaseName))
	{
		m_authorsModelController.RegisterObserver(this);
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
		m_qmlEngine.load("qrc:/qml/Main.qml");
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
		return &m_authorsModelController;
	}

private: // ModelControllerObserver
	void HandleCurrentIndexChanged(ModelController * const controller, const int /*index*/) override
	{
		m_focusedController = controller;
	}

private:
	GuiController & m_self;
	QQmlApplicationEngine m_qmlEngine;
	PropagateConstPtr<DB::Database> m_db;
	AuthorsModelController m_authorsModelController { *m_db };
	ModelController * m_focusedController { &m_authorsModelController };
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

bool GuiController::GetRunning() const noexcept
{
	return m_impl->GetRunning();
}

}
