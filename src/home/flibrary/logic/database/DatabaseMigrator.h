#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseMigrator.h"
#include "interface/logic/IDatabaseUser.h"

namespace HomeCompa::Flibrary
{

class DatabaseMigrator final : virtual public IDatabaseMigrator
{
	NON_COPY_MOVABLE(DatabaseMigrator)

public:
	DatabaseMigrator(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<const ICollectionProvider> collectionProvider);
	~DatabaseMigrator() override;

private: // IDatabaseMigrator
	bool NeedMigrate() const override;
	void Migrate() override;
	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
