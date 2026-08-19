#pragma once

#include <functional>

#include "interface/localization.h"

class QString;

namespace HomeCompa::Flibrary::UserData
{

using Callback = std::function<void(const QString&)>;

inline constexpr auto CONTEXT          = "UserData";
inline constexpr auto CANNOT_READ_FROM = QT_TRANSLATE_NOOP("UserData", "Cannot read from %1");

TR_DEF

}
