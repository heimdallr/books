#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/ILogController.h"

namespace HomeCompa::Flibrary {

class LogController : virtual public ILogController
{
	NON_COPY_MOVABLE(LogController)

public:
	explicit LogController(std::shared_ptr<class DatabaseUser> databaseUser);
	~LogController() override;

private:
	QAbstractItemModel * GetModel() const noexcept override;
	void Clear() override;
	std::vector<const char *> GetSeverities() const override;
	int GetSeverity() const override;
	void SetSeverity(int value) override;
	void ShowCollectionStatistics() const override;
	void TestColors() const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
