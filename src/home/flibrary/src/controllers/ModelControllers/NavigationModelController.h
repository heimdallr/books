#pragma once

#include "fnd/memory.h"

#include "models/NavigationItem.h"

#include "ModelController.h"

namespace HomeCompa::Util {
class Executor;
}

namespace HomeCompa::DB {
class Database;
}

namespace HomeCompa::Flibrary {

class NavigationModelController
	: public ModelController
{
	NON_COPY_MOVABLE(NavigationModelController)
public:
	NavigationModelController(Util::Executor & executor, DB::Database & db);

	~NavigationModelController() override;

private: // ModelController
	Type GetType() const noexcept override;

private:
	NavigationItems m_authors;
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
