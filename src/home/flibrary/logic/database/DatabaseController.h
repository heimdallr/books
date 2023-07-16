#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa::DB {
class IDatabase;
}

namespace HomeCompa::Flibrary {

class DatabaseController final
{
	NON_COPY_MOVABLE(DatabaseController)

public:
	explicit DatabaseController(std::shared_ptr<class ICollectionController> collectionController);
	~DatabaseController();

public:
	std::unique_ptr<DB::IDatabase> CreateDatabase() const;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
