#pragma once

#include <memory>

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IDatabaseProvider.h"
#include "interface/logic/IUserDataController.h"
#include "interface/ui/IUiFactory.h"

#include "constants/UserData.h"

class QIODevice;
class QString;

namespace HomeCompa::DB {

class IDatabase;

}

namespace HomeCompa::Util {

class IExecutor;

}

namespace HomeCompa::Flibrary {

class UserDataController final : public IUserDataController
{
	NON_COPY_MOVABLE(UserDataController)

public:
	UserDataController(std::shared_ptr<const IDatabaseProvider> databaseProvider, std::shared_ptr<IUiFactory> uiFactory);
	~UserDataController() override;

private: // IUserDataController
	void Backup(Callback callback) const override;
	void Restore(Callback callback) const override;
	void Backup(const QString& path, Callback callback) const override;
	void Restore(const QString& path, Callback callback) const override;
	void Backup(QIODevice& stream, Callback callback) const override;
	void Restore(QIODevice& stream, RestoreMode mode, Callback callback) const override;

private:
	using DoFunction = void (*)(const IDatabaseProvider& databaseProvider, QIODevice& stream, UserData::Callback callback);
	void Do(Callback callback, QIODevice& stream, const char* successMessage, DoFunction f) const;

private:
	std::shared_ptr<const IDatabaseProvider> m_databaseProvider;
	std::shared_ptr<const IUiFactory>        m_uiFactory;
};

} // namespace HomeCompa::Flibrary
