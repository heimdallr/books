#pragma once

#include <filesystem>
#include <map>
#include <string>

#include "InpxLibLib.h"

namespace HomeCompa::Inpx {

INPXLIB_API bool CreateNewCollection(const std::filesystem::path & iniFile);
INPXLIB_API bool CreateNewCollection(std::map<std::wstring, std::filesystem::path> data);
INPXLIB_API bool CheckUpdateCollection(std::map<std::wstring, std::filesystem::path> data);
INPXLIB_API bool UpdateCollection(std::map<std::wstring, std::filesystem::path> data);

}
