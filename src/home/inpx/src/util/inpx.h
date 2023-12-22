#pragma once

#include <filesystem>
#include <map>
#include <string>

#include "fnd/EnumBitmask.h"

#include "export/inpxlib.h"

namespace HomeCompa::Inpx {

enum class CreateCollectionMode
{
	None                 = 0,
	AddUnIndexedFiles    = 1 << 0,
	ScanUnIndexedFolders = 1 << 1,
};

INPXLIB_EXPORT bool CreateNewCollection(const std::filesystem::path & iniFile);
INPXLIB_EXPORT bool CreateNewCollection(std::map<std::wstring, std::filesystem::path> data, CreateCollectionMode mode);
INPXLIB_EXPORT bool UpdateCollection(std::map<std::wstring, std::filesystem::path> data, CreateCollectionMode mode);

}

ENABLE_BITMASK_OPERATORS(HomeCompa::Inpx::CreateCollectionMode);
