#include "DatabaseChecker.h"

#include "DatabaseUser.h"

using namespace HomeCompa::Flibrary;

DatabaseChecker::DatabaseChecker(std::shared_ptr<DatabaseUser> databaseUser)
	: m_databaseUser(std::move(databaseUser))
{
}

bool DatabaseChecker::IsDatabaseValid() const
{
	return !!m_databaseUser->Database();
}
