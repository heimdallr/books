#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IDatabaseController.h"

namespace HomeCompa::Flibrary
{

class DatabaseController final : virtual public IDatabaseController
{
	NON_COPY_MOVABLE(DatabaseController)

public:
	explicit DatabaseController(std::shared_ptr<class ICollectionProvider> collectionProvider);
	~DatabaseController() override;

public:
	std::shared_ptr<DB::IDatabase> GetDatabase(bool readOnly) const override;
	std::shared_ptr<DB::IDatabase> CheckDatabase() const override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
