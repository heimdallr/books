#pragma once

#include <QByteArray>
#include "export/JxlWrapper.h"

class QImage;

namespace HomeCompa::JXL
{

JXLWRAPPER_EXPORT QByteArray Encode(const QImage& image, int quality);
JXLWRAPPER_EXPORT QImage Decode(const QByteArray& bytes);

}
