#pragma once

#include "interface/logic/IScriptController.h"

namespace HomeCompa::Flibrary
{

class CommandExecutor final : public IScriptController::ICommandExecutor
{
public:
	bool ExecuteSystem(const IScriptController::Command& command) const override;
	bool ExecuteLaunchConsoleApp(const IScriptController::Command& command) const override;
	bool ExecuteLaunchGuiApp(const IScriptController::Command& command) const override;
};

}
