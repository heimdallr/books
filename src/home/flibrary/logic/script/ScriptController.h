#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IScriptController.h"
#include "util/ISettings.h"

namespace Hypodermic
{
class Container;
}

namespace HomeCompa::Flibrary
{

class ScriptController final : virtual public IScriptController
{
	NON_COPY_MOVABLE(ScriptController)

public:
	ScriptController(std::shared_ptr<ISettings> settings, const std::shared_ptr<const ICommandExecutor>& commandExecutor);
	~ScriptController() override;

private: // IScriptController
	const Scripts& GetScripts() const noexcept override;
	bool InsertScripts(int row, int count) override;
	bool RemoveScripts(int row, int count) override;
	bool SetScriptType(int n, Script::Type value) override;
	bool SetScriptName(int n, QString value) override;
	bool SetScriptNumber(int n, int value) override;

	const Commands& GetCommands() const noexcept override;
	Commands GetCommands(const QString& scriptUid) const override;
	bool InsertCommand(const QString& uid, int row, int count) override;
	bool RemoveCommand(int row, int count) override;
	bool SetCommandType(int n, Command::Type value) override;
	bool SetCommandCommand(int n, QString value) override;
	bool SetCommandArgs(int n, QString value) override;
	bool SetCommandNumber(int n, int value) override;

	bool Execute(const Command& command) const override;
	void Save() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

class ScriptControllerProvider : virtual public IScriptControllerProvider
{
public:
	explicit ScriptControllerProvider(Hypodermic::Container& container);

private: // IScriptControllerProvider
	std::shared_ptr<IScriptController> GetScriptController() override;

private:
	Hypodermic::Container& m_container;
	std::weak_ptr<IScriptController> m_controller;
};

} // namespace HomeCompa::Flibrary
