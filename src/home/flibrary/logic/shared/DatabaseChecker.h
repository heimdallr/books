#pragma once

#include "fnd/memory.h"

#include "interface/logic/IDatabaseChecker.h"

namespace HomeCompa::Flibrary {

class DatabaseUser;

class DatabaseChecker final : virtual public IDatabaseChecker
{
public:
	explicit DatabaseChecker(std::shared_ptr<DatabaseUser> databaseUser);

private: // IDatabaseChecker
	bool IsDatabaseValid() const override;

private:
	PropagateConstPtr<DatabaseUser, std::shared_ptr> m_databaseUser;
};

}
