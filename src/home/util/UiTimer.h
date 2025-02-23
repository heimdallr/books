#pragma once

#include <functional>
#include <memory>

#include "export/util.h"

class QTimer;

namespace HomeCompa::Util
{

UTIL_EXPORT std::unique_ptr<QTimer> CreateUiTimer(std::function<void()> f);

}
