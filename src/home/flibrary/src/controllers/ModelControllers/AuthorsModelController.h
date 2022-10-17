#pragma once

#include "fnd/memory.h"

#include "models/Author.h"

#include "ModelController.h"

namespace HomeCompa {
class Executor;
}

namespace HomeCompa::DB {
class Database;
}

namespace HomeCompa::Flibrary {

class AuthorsModelController
	: public ModelController
{
	NON_COPY_MOVABLE(AuthorsModelController)
public:
	AuthorsModelController(Executor & executor, DB::Database & db);

	~AuthorsModelController() override;

private: // ModelController
	Type GetType() const noexcept override;

private:
	Authors m_authors;
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
