#include <QString>

#include "AppVersion.h"

#include "build.h"
#include "version.h"

namespace HomeCompa::Flibrary {

QString GetApplicationVersion()
{
	return QString("%1.%2").arg(PRODUCT_VERSION).arg(BUILD_NUMBER);
}

}
