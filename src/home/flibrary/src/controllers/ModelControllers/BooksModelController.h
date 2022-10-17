#pragma once

#include "ModelController.h"

namespace HomeCompa::DB {
class Database;
}

namespace HomeCompa::Flibrary {

class BooksModelController
	: public ModelController
{
	NON_COPY_MOVABLE(BooksModelController)
public:
	explicit BooksModelController(DB::Database & db);
	~BooksModelController() override;

	void SetAuthorId(int authorId);

private: // ModelController
	Type GetType() const noexcept override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
