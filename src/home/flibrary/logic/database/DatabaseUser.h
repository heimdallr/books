#pragma once

#include <memory>

#include <QTimer>

#include "fnd/NonCopyMovable.h"

#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/ILogicFactory.h"

namespace HomeCompa::Flibrary {

class DatabaseUser final : public IDatabaseUser
{
	NON_COPY_MOVABLE(DatabaseUser)

public:
	DatabaseUser(const std::shared_ptr<ILogicFactory>& logicFactory, std::shared_ptr<const ICollectionProvider> collectionProvider);
	~DatabaseUser() override;

public:
	size_t                           Execute(Util::IExecutor::Task&& task, int priority = 0) const override;
	void                             Reset() override;
	std::shared_ptr<DB::IDatabase>   GetDatabase() const override;
	std::shared_ptr<DB::IDatabase>   CheckDatabase() const override;
	std::shared_ptr<Util::IExecutor> Executor() const override;
	void                             EnableApplicationCursorChange(bool value) override;
	QVariant                         GetSetting(Key key, QVariant defaultValue) const override;
	void                             SetSetting(Key key, const QVariant& value) const override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
