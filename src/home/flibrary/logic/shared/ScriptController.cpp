#include "ScriptController.h"

#include <plog/Log.h>

using namespace HomeCompa::Flibrary;

class ScriptController::Impl
{
};

ScriptController::ScriptController()
{
	PLOGD << "ScriptController created";
}

ScriptController::~ScriptController()
{
	PLOGD << "ScriptController destroyed";
}
