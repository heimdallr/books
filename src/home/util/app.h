#pragma once

#include "export/util.h"

namespace HomeCompa::Util
{

#define INSTALLER_MODE_ITEMS_X_MACRO \
	INSTALLER_MODE_ITEM(exe, "exe")  \
	INSTALLER_MODE_ITEM(msi, "msi")  \
	INSTALLER_MODE_ITEM(portable, "7z")

enum class InstallerType
{
#define INSTALLER_MODE_ITEM(NAME, _) NAME,
	INSTALLER_MODE_ITEMS_X_MACRO
#undef INSTALLER_MODE_ITEM
};

struct InstallerDescription
{
	InstallerType type;
	const char*   name;
	const char*   ext;
};

UTIL_EXPORT InstallerDescription GetInstallerDescription();

}
