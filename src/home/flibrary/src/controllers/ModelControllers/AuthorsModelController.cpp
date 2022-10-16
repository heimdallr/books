#include <QAbstractItemModel>
#include <QQmlEngine>

#include "AuthorsModelController.h"

namespace HomeCompa::Flibrary {

QAbstractItemModel * CreateAuthorsModel(DB::Database & db, QObject * parent = nullptr);

AuthorsModelController::AuthorsModelController(DB::Database & db)
	: m_model(std::unique_ptr<QAbstractItemModel>(CreateAuthorsModel(db)))
{
	QQmlEngine::setObjectOwnership(m_model.get(), QQmlEngine::CppOwnership);
	QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
	ResetModel(m_model.get());
}

AuthorsModelController::~AuthorsModelController()
{
	ResetModel();
}

ModelController::Type AuthorsModelController::GetType() const noexcept
{
	return Type::Authors;
}

}
