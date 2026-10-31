#include "DatabaseChecker.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

DatabaseChecker::DatabaseChecker(std::shared_ptr<const IDatabaseProvider> databaseProvider)
	: m_databaseProvider(std::move(databaseProvider))
{
	PLOGV << "DatabaseChecker created";
}

DatabaseChecker::~DatabaseChecker()
{
	PLOGV << "DatabaseChecker destroyed";
}

bool DatabaseChecker::IsDatabaseValid() const
{
	return !!m_databaseProvider->GetDatabase();
}
