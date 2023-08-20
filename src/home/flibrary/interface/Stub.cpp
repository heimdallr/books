#include "constants/Localization.h"

namespace HomeCompa::Flibrary::Loc {

QString Tr(const char * context, const char * str)
{
	return QCoreApplication::translate(context, str);
}

}
