#include "fnd/FindPair.h"

#include "constants/Localization.h"
#include "logic/IScriptController.h"

namespace HomeCompa::Flibrary::Loc {

QString Tr(const char * context, const char * str)
{
	return QCoreApplication::translate(context, str);
}

}

namespace HomeCompa::Flibrary {

bool IScriptController::Command::HasMacro(const Macro macro) const
{
	return args.contains(FindSecond(s_commandMacros, macro));
}

void IScriptController::Command::SetMacro(const Macro macro, const QString & value)
{
	args.replace(FindSecond(s_commandMacros, macro), value);
}

}
