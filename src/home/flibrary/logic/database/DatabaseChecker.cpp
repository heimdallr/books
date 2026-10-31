#include "DatabaseChecker.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

DatabaseChecker::DatabaseChecker(std::shared_ptr<const IDatabaseProvider> databaseUser)
	: m_databaseUser(std::move(databaseUser))
{
	PLOGV << "DatabaseChecker created";
}

DatabaseChecker::~DatabaseChecker()
{
	PLOGV << "DatabaseChecker destroyed";
}

bool DatabaseChecker::IsDatabaseValid() const
{
	return !!m_databaseUser->GetDatabase();
}
