#pragma once

#include <filesystem>
#include <map>
#include <string>

#include "export/inpxlib.h"

namespace HomeCompa::Inpx {

INPXLIB_EXPORT bool CreateNewCollection(const std::filesystem::path & iniFile);
INPXLIB_EXPORT bool CreateNewCollection(std::map<std::wstring, std::filesystem::path> data);
INPXLIB_EXPORT bool UpdateCollection(std::map<std::wstring, std::filesystem::path> data);

}
