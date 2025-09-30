#pragma once

#include <memory>

#include "fnd/NonCopyMovable.h"

#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IProgressController.h"
#include "interface/logic/IUpdateChecker.h"
#include "interface/ui/IUiFactory.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class UpdateChecker final : virtual public IUpdateChecker
{
	NON_COPY_MOVABLE(UpdateChecker)

public:
	UpdateChecker(
		const std::shared_ptr<const ILogicFactory>&        logicFactory,
		std::shared_ptr<ISettings>                         settings,
		std::shared_ptr<IUiFactory>                        uiFactory,
		std::shared_ptr<IBooksExtractorProgressController> progressController
	);
	~UpdateChecker() override;

private: // IUpdateChecker
	void CheckForUpdate(bool force, Callback callback) override;

private:
	class Impl;
	std::shared_ptr<Impl> m_impl;
};

}
