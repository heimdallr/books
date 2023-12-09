#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/IScriptController.h"

namespace HomeCompa::Flibrary {

class ScriptController final
	: virtual public IScriptController
{
	NON_COPY_MOVABLE(ScriptController)

public:
	ScriptController();
	~ScriptController() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
