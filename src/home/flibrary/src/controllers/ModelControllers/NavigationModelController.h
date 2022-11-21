#pragma once

#include "fnd/ConvertableT.h"
#include "fnd/memory.h"

#include "models/NavigationModelObserver.h"

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
	, public NavigationModelObserver
	, public ConvertibleT<NavigationModelController>
{
	NON_COPY_MOVABLE(NavigationModelController)
	Q_OBJECT

public:
	NavigationModelController(Util::Executor & executor
		, DB::Database & db
		, NavigationSource navigationSource
		, Settings & uiSettings
	);

	~NavigationModelController() override;

private: // ModelController
	Type GetType() const noexcept override;
	QAbstractItemModel * CreateModel() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
