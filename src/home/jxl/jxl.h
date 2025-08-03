#pragma once

#include <QByteArray>

#include "export/fljxl.h"

class QImage;

namespace HomeCompa::JXL
{

FLJXL_EXPORT QByteArray Encode(const QImage& image, int quality);
FLJXL_EXPORT QImage Decode(const QByteArray& bytes);

}
