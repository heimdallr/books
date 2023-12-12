#pragma once

#include "interface/logic/IScriptController.h"

namespace HomeCompa::Flibrary {

class CommandExecutor final : public IScriptController::ICommandExecutor
{
public:
	bool ExecuteSystem(const IScriptController::Command & command) const override;
	bool ExecuteLaunchApp(const IScriptController::Command & command) const override;
};

}
