#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/IScriptController.h"

namespace Hypodermic {
class Container;
}

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class ScriptController final
	: virtual public IScriptController
{
	NON_COPY_MOVABLE(ScriptController)

public:
	explicit ScriptController(std::shared_ptr<ISettings> settings);
	~ScriptController() override;

private: // IScriptController
	const Scripts & GetScripts() const noexcept override;
	bool InsertScripts(int row, int count) override;
	bool RemoveScripts(int row, int count) override;
	bool SetScriptType(int n, Script::Type type) override;
	bool SetScriptName(int n, QString name) override;
	bool SetScriptNumber(int n, int number) override;

	const Commands & GetCommands() const noexcept override;
	bool InsertCommand(const QString & uid, int row, int count) override;

	void Save() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

class ScriptControllerProvider : virtual public IScriptControllerProvider
{
public:
	explicit ScriptControllerProvider(Hypodermic::Container & container);

private: // IScriptControllerProvider
	std::shared_ptr<IScriptController> GetScriptController() override;

private:
	Hypodermic::Container & m_container;
	std::weak_ptr<IScriptController> m_controller;
};

}
