#pragma once

#include <functional>

#include "export/logic.h"

class QIODevice;
class QByteArray;
class QString;

namespace HomeCompa {
class Zip;
}

namespace HomeCompa::Flibrary {

LOGIC_EXPORT QByteArray RestoreImages(QIODevice & input, const QString & folder, const QString & fileName);

using ExtractBookImagesCallback = std::function<bool(QString, QByteArray)>;
bool ExtractBookImages(const QString & folder, const QString & fileName, const ExtractBookImagesCallback & callback);

}
