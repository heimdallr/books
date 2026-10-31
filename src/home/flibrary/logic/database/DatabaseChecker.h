#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IDatabaseChecker.h"
#include "interface/logic/IDatabaseProvider.h"

namespace HomeCompa::Flibrary {

class DatabaseChecker final : public IDatabaseChecker
{
	NON_COPY_MOVABLE(DatabaseChecker)

public:
	explicit DatabaseChecker(std::shared_ptr<const IDatabaseProvider> databaseProvider);
	~DatabaseChecker() override;

private: // IDatabaseChecker
	bool IsDatabaseValid() const override;

private:
	std::shared_ptr<const IDatabaseProvider> m_databaseProvider;
};

} // namespace HomeCompa::Flibrary
