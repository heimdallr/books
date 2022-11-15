#pragma once

#include <filesystem>
#include <map>
#include <string>

#include "InpxLibLib.h"

namespace HomeCompa::Inpx {

INPXLIB_API int ParseInpx(const std::filesystem::path & iniFile);
INPXLIB_API int ParseInpx(std::map<std::wstring, std::wstring> data);

}
