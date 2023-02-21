#pragma once

#include "fnd/ConvertableT.h"
#include "fnd/memory.h"

#include "models/NavigationModelObserver.h"

#include "ModelController.h"

#include "NavigationSource.h"

namespace HomeCompa::Util {
class IExecutor;
}

namespace HomeCompa::DB {
class IDatabase;
}

namespace HomeCompa::Flibrary {

class NavigationModelController final
	: public ModelController
	, public NavigationModelObserver
	, public ConvertibleT<NavigationModelController>
{
	NON_COPY_MOVABLE(NavigationModelController)
	Q_OBJECT

public:
	NavigationModelController(Util::IExecutor & executor
		, DB::IDatabase & db
		, NavigationSource navigationSource
		, Settings & uiSettings
	);

	~NavigationModelController() override;

private: // ModelController
	ModelControllerType GetType() const noexcept override;
	void UpdateModelData() override;
	QAbstractItemModel * CreateModel() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
