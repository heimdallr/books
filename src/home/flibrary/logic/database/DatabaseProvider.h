#pragma once

#include <memory>

#include "fnd/NonCopyMovable.h"

#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseProvider.h"
#include "interface/logic/ILogicFactory.h"

namespace HomeCompa::Flibrary {

class DatabaseProvider final : public IDatabaseController
{
	NON_COPY_MOVABLE(DatabaseProvider)

public:
	DatabaseProvider(const std::shared_ptr<ILogicFactory>& logicFactory, std::shared_ptr<const ICollectionProvider> collectionProvider);
	~DatabaseProvider() override;

public:
	size_t                           Execute(Util::IExecutor::Task&& task, int priority = 0) const override;
	void                             Reset() override;
	std::shared_ptr<DB::IDatabase>   GetDatabase() const override;
	std::shared_ptr<Util::IExecutor> Executor() const override;
	void                             EnableApplicationCursorChange(bool value) override;
	QVariant                         GetSetting(Key key, QVariant defaultValue) const override;
	void                             SetSetting(Key key, const QVariant& value) const override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
