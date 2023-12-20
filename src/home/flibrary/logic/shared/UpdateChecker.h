#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/IUpdateChecker.h"

namespace HomeCompa::Flibrary {

class UpdateChecker final
	: virtual public IUpdateChecker
{
	NON_COPY_MOVABLE(UpdateChecker)

public:
	UpdateChecker();
	~UpdateChecker() override;

private: // IUpdateChecker
	void CheckForUpdate(Callback callback) override;

private:
	struct Impl;
	std::shared_ptr<Impl> m_impl;
};

}
