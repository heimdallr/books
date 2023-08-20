#include <QString>

#include "AppVersion.h"

#include "build.h"
#include "version.h"

namespace HomeCompa::Flibrary {

QString GetApplicationVersion()
{
	return QString("%1.%2.%3").arg(PRODUCT_VERSION_MAJOR).arg(PRODUCT_VERSION_MINOR).arg(BUILD_NUMBER);
}

}
