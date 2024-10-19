#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/IUserDataController.h"
#include "constants/UserData.h"

class QString;

namespace HomeCompa::DB {
class IDatabase;
}

namespace HomeCompa::Util {
class IExecutor;
}

namespace HomeCompa::Flibrary {

class UserDataController : virtual public IUserDataController
{
	NON_COPY_MOVABLE(UserDataController)

public:
	UserDataController(std::shared_ptr<class DatabaseUser> databaseUser
		, std::shared_ptr<class IUiFactory> uiFactory
	);
	~UserDataController() override;
	
private: // IUserDataController
	void Backup(Callback callback) const override;
	void Restore(Callback callback) const override;

private:
	using DoFunction = void(*)(const Util::IExecutor & executor, DB::IDatabase & db, QString fileName, UserData::Callback callback);
	void Do(Callback callback, QString fileName, const char * successMessage, DoFunction f) const;

private:
	std::shared_ptr<const DatabaseUser> m_databaseUser;
	std::shared_ptr<const IUiFactory> m_uiFactory;
};

}
