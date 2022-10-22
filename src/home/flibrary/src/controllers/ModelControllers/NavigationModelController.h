#pragma once

#include "fnd/memory.h"

#include "ModelController.h"

#include "NavigationSource.h"

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
	NavigationModelController(Util::Executor & executor, DB::Database & db, NavigationSource navigationSource);

	~NavigationModelController() override;

private: // ModelController
	Type GetType() const noexcept override;
	QAbstractItemModel * CreateModel() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
