#pragma once

#include <QRect>

#include "export/gutil.h"

class QWidget;

namespace HomeCompa::Util
{

QRect GUTIL_EXPORT GetGlobalGeometry(const QWidget& widget);

}
