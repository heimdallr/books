#pragma once

#include <memory>

class QByteArray;
class QString;

namespace HomeCompa {
class Zip;
}

namespace HomeCompa::Flibrary {

QByteArray RestoreImages(const QByteArray & input, const QString & folder, const QString & fileName);
std::unique_ptr<Zip> CreateImageArchive(const QString & folder);

}
