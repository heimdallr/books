#pragma once

#include "fnd/memory.h"

#include "ModelController.h"

#include "models/Book.h"

namespace HomeCompa {
class Executor;
}

namespace HomeCompa::DB {
class Database;
}

namespace HomeCompa::Flibrary {

class BooksModelController
	: public ModelController
{
	NON_COPY_MOVABLE(BooksModelController)
public:
	BooksModelController(Executor & executor, DB::Database & db);
	~BooksModelController() override;

	void SetAuthorId(int authorId);

private: // ModelController
	Type GetType() const noexcept override;

private:
	Books m_books;
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
