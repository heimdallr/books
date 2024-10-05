#pragma once

#include <functional>

class QIODevice;
class QByteArray;
class QString;

namespace HomeCompa {
class Zip;
}

namespace HomeCompa::Flibrary {

QByteArray RestoreImages(QIODevice & input, const QString & folder, const QString & fileName);

using ExtractBookImagesCallback = std::function<bool(QString, QByteArray)>;
bool ExtractBookImages(const QString & folder, const QString & fileName, const ExtractBookImagesCallback & callback);

}
