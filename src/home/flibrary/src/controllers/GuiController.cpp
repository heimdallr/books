//#include <QAbstractItemModel>
#include <QQmlEngine>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "fnd/memory.h"

#include "database/factory/Factory.h"

#include "database/interface/Database.h"

#include "models/AuthorsModel.h"

#include "GuiController.h"

namespace HomeCompa::Flibrary {

class GuiController::Impl
{
public:
	explicit Impl(const std::string & databaseName)
		: m_db(Create(DB::Factory::Impl::Sqlite, databaseName))
		, m_authorsModel(std::unique_ptr<QAbstractItemModel>(CreateAuthorsModel(*m_db)))
	{
		QQmlEngine::setObjectOwnership(m_authorsModel.get(), QQmlEngine::CppOwnership);
	}

	~Impl()
	{
		m_qmlEngine.clearComponentCache();
	}

	void Start(GuiController * guiController)
	{
		auto * const qmlContext = m_qmlEngine.rootContext();
		qmlContext->setContextProperty("guiController", guiController);
		qRegisterMetaType<QAbstractItemModel *>("QAbstractItemModel*");
		m_qmlEngine.load("qrc:/qml/Main.qml");
	}
	
	QAbstractItemModel * GetAuthorsModel()
	{
		return m_authorsModel.get();
	}

private:	
	QQmlApplicationEngine m_qmlEngine;
	PropagateConstPtr<DB::Database> m_db;
	PropagateConstPtr<QAbstractItemModel> m_authorsModel;
};

GuiController::GuiController(const std::string & databaseName, QObject * parent)
	: QObject(parent)
	, m_impl(databaseName)
{
}

GuiController::~GuiController() = default;

void GuiController::Start()
{
	m_impl->Start(this);
}

QAbstractItemModel * GuiController::GetAuthorsModel()
{
	return m_impl->GetAuthorsModel();
}

void GuiController::OnCurrentAuthorChanged(int /*index*/)
{
}

}
