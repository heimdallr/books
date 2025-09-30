#include "app.h"

#include <QCoreApplication>
#include <QFile>
#include <QString>

#include "log.h"

namespace HomeCompa::Util
{

constexpr InstallerDescription MODES[] {
#define INSTALLER_MODE_ITEM(NAME, EXT) { InstallerType::NAME, #NAME, EXT },
	INSTALLER_MODE_ITEMS_X_MACRO
#undef INSTALLER_MODE_ITEM

};

InstallerDescription GetInstallerDescription()
{
	const auto fileNamePortable = QString("%1/installer_mode").arg(QCoreApplication::applicationDirPath());
	QFile      file(QString("%1/installer_mode").arg(QCoreApplication::applicationDirPath()));
	if (!file.open(QIODevice::ReadOnly))
	{
		PLOGD << "Installer mode: exe";
		return MODES[0];
	}

	const auto bytes = file.readAll();
	PLOGD << "Installer mode: " << bytes;
	const auto it = std::ranges::find_if(MODES, [&](const auto& item) {
		return bytes.startsWith(item.name);
	});
	return it != std::end(MODES) ? *it : MODES[0];
}

} // namespace HomeCompa::Util
