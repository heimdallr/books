#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/IUpdateChecker.h"

namespace HomeCompa {
class ISettings;
}
namespace HomeCompa::Flibrary {

class UpdateChecker final
	: virtual public IUpdateChecker
{
	NON_COPY_MOVABLE(UpdateChecker)

public:
	UpdateChecker(std::shared_ptr<ISettings> settings
		, const std::shared_ptr<const class ILogicFactory>& logicFactory
		, std::shared_ptr<class IUiFactory> uiFactory
		, std::shared_ptr<class IBooksExtractorProgressController> progressController
	);
	~UpdateChecker() override;

private: // IUpdateChecker
	void CheckForUpdate(bool force, Callback callback) override;

private:
	class Impl;
	std::shared_ptr<Impl> m_impl;
};

}
