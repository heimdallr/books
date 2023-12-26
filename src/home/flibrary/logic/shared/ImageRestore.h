#pragma once

#include <memory>

class QIODevice;
class QByteArray;
class QString;

namespace HomeCompa {
class Zip;
}

namespace HomeCompa::Flibrary {

QByteArray RestoreImages(QIODevice & input, const QString & folder, const QString & fileName);
std::unique_ptr<Zip> CreateImageArchive(const QString & folder);

}
