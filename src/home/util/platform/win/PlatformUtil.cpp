#include "PlatformUtil.h"

#include <QSettings>

namespace HomeCompa::Util
{

bool IsRegisteredExtension(const QString& extension)
{
	const QSettings m("HKEY_CLASSES_ROOT", QSettings::NativeFormat);
	const auto fileType = m.value(QString(".%1/.").arg(extension)).toString();
	if (fileType.isEmpty())
		return false;

	const auto command = m.value(QString("%1/shell/open/command/.").arg(fileType)).toString();
	return !command.isEmpty();
}

}
