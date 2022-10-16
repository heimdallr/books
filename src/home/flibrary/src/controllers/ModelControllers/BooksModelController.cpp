#include <QAbstractItemModel>
#include <QQmlEngine>

#include "BooksModelController.h"

namespace HomeCompa::Flibrary {

QAbstractItemModel * CreateAuthorsModel(DB::Database & db, QObject * parent = nullptr);

BooksModelController::BooksModelController(DB::Database & db)
	: m_db(db)
{
	QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

BooksModelController::~BooksModelController()
{
	ResetModel();
}

ModelController::Type BooksModelController::GetType() const noexcept
{
	return Type::Books;
}

}
