#include <set>

#include <plog/Log.h>

#include "Collection.h"
#include "CollectionController.h"

namespace HomeCompa::Flibrary {

namespace {
}

struct CollectionController::Impl
{
	std::shared_ptr<ISettings> settings;
	Collections collections { Collection::Deserialize(*settings) };

	explicit Impl(std::shared_ptr<ISettings> settings)
		: settings(std::move(settings))
	{
	}
};

CollectionController::CollectionController(std::shared_ptr<ISettings> settings)
	: m_impl(std::move(settings))
{
	PLOGD << "CollectionController created";
}

CollectionController::~CollectionController()
{
	PLOGD << "CollectionController destroyed";
}

}
