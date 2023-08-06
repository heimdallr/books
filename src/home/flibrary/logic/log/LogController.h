#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/ILogController.h"

namespace HomeCompa::Flibrary {

class LogController : virtual public ILogController
{
	NON_COPY_MOVABLE(LogController)

public:
	LogController();
	~LogController() override;

private:
	QAbstractItemModel * GetModel() const noexcept override;
	void Clear() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
