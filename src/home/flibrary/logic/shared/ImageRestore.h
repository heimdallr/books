#pragma once

#include <functional>
#include <memory>

#include "export/logic.h"

namespace HomeCompa
{
class ISettings;
}

class QByteArray;
class QIODevice;
class QPixmap;
class QString;

namespace HomeCompa
{
class Zip;
}

namespace HomeCompa::Flibrary
{

constexpr auto IMAGE_JPEG = "image/jpeg";
constexpr auto IMAGE_PNG = "image/png";

LOGIC_EXPORT QPixmap Decode(const QByteArray& bytes);
LOGIC_EXPORT std::pair<QByteArray, const char*> Recode(const QByteArray& bytes);
LOGIC_EXPORT QByteArray RestoreImages(QIODevice& input, const QString& folder, const QString& fileName, const std::shared_ptr<const ISettings>& settings = {});

using ExtractBookImagesCallback = std::function<bool(QString, QByteArray)>;
void ExtractBookImages(const QString& folder, const QString& fileName, const ExtractBookImagesCallback& callback, const std::shared_ptr<const ISettings>& settings = {});

}
