#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IDatabaseChecker.h"

namespace HomeCompa::Flibrary
{

class DatabaseUser;

class DatabaseChecker final : virtual public IDatabaseChecker
{
	NON_COPY_MOVABLE(DatabaseChecker)

public:
	explicit DatabaseChecker(std::shared_ptr<DatabaseUser> databaseUser);
	~DatabaseChecker() override;

private: // IDatabaseChecker
	bool IsDatabaseValid() const override;

private:
	std::shared_ptr<const DatabaseUser> m_databaseUser;
};

}
