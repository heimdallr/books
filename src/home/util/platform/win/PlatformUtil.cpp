#include "PlatformUtil.h"

#include <QSettings>

namespace HomeCompa::Util
{

bool IsRegisteredExtension(const QString& extension)
{
	QSettings  m("HKEY_CLASSES_ROOT", QSettings::NativeFormat);
	const auto fileType = m.value(QString(".%1/.").arg(extension)).toString();
	if (fileType.isEmpty())
	{
		m.beginGroup(QString(".%1/OpenWithProgids").arg(extension));
		return !m.allKeys().isEmpty();
	}

	const auto command = m.value(QString("%1/shell/open/command/.").arg(fileType)).toString();
	return !command.isEmpty();
}

}
