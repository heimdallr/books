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

bool IScriptController::HasMacro(const QString & str, const Macro macro)
{
	return str.contains(GetMacro(macro));
}

QString & IScriptController::SetMacro(QString & str, const Macro macro, const QString & value)
{
	return str.replace(GetMacro(macro), value);
}

const char * IScriptController::GetMacro(const Macro macro)
{
	return FindSecond(s_commandMacros, macro);
}

}
