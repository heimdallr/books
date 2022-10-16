#pragma once

#include "fnd/memory.h"

#include "ModelController.h"

namespace HomeCompa::DB {
class Database;
}

namespace HomeCompa::Flibrary {

class AuthorsModelController
	: public ModelController
{
	NON_COPY_MOVABLE(AuthorsModelController)
public:
	explicit AuthorsModelController(DB::Database & db);

	~AuthorsModelController() override;

private: // ModelController
	Type GetType() const noexcept override;

private:
	PropagateConstPtr<QAbstractItemModel> m_model;
};

}