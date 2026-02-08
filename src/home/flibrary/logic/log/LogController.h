#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/ILogController.h"

namespace HomeCompa::Flibrary
{

class LogController final : virtual public ILogController
{
	NON_COPY_MOVABLE(LogController)

public:
	LogController(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<const ICollectionProvider> collectionProvider);
	~LogController() override;

private:
	QAbstractItemModel*      GetModel() const noexcept override;
	void                     Clear() override;
	std::vector<const char*> GetSeverities() const override;
	int                      GetSeverity() const override;
	void                     SetSeverity(int value) override;
	void                     ShowCollectionStatistics() const override;
	void                     TestColors() const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
