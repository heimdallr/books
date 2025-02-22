#pragma once

#include <QRect>

#include "export/GuiUtil.h"

class QWidget;

namespace HomeCompa::Util
{

QRect GUIUTIL_EXPORT GetGlobalGeometry(const QWidget& widget);

}
