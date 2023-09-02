#pragma once

#include <functional>
#include <memory>

#include "util_export.h"

class QTimer;

namespace HomeCompa::Util {

UTIL_EXPORT std::unique_ptr<QTimer> CreateUiTimer(std::function<void()> f);

}
