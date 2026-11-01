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

private: // IDatabaseProvider
	size_t Execute(Task<DB::IDatabase>&& task, int priority = 0) const override;
	size_t ExecuteWritable(Task<DB::IDatabaseWritable>&& task, int priority = 0) const override;

	bool                                   IsDatabaseValid() const noexcept override;
	std::shared_ptr<DB::IDatabase>         GetDatabase() const override;
	std::unique_ptr<DB::IDatabaseWritable> GetDatabaseWritable() const override;
	QVariant                               GetSetting(Key key, QVariant defaultValue) const override;
	void                                   SetSetting(Key key, const QVariant& value) const override;

private: // IDatabaseController
	void Reset() override;
	void EnableApplicationCursorChange(bool value) override;
	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
