#pragma once

#include <functional>
#include <memory>

#include "UtilLib.h"

class QTimer;

namespace HomeCompa::Util {

UTIL_API std::unique_ptr<QTimer> CreateUiTimer(std::function<void()> f);

}
