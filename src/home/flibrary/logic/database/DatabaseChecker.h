#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IDatabaseChecker.h"
#include "interface/logic/IDatabaseUser.h"

namespace HomeCompa::Flibrary
{

class DatabaseUser;

class DatabaseChecker final : virtual public IDatabaseChecker
{
	NON_COPY_MOVABLE(DatabaseChecker)

public:
	explicit DatabaseChecker(std::shared_ptr<IDatabaseUser> databaseUser);
	~DatabaseChecker() override;

private: // IDatabaseChecker
	bool IsDatabaseValid() const override;

private:
	std::shared_ptr<const IDatabaseUser> m_databaseUser;
};

}
