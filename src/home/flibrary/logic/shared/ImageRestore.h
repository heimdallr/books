#pragma once

#include <functional>
#include <memory>

#include "export/logic.h"

class QByteArray;
class QImage;
class QIODevice;
class QPixmap;
class QString;

namespace HomeCompa
{
class ISettings;
class Zip;
}

namespace HomeCompa::Flibrary
{

LOGIC_EXPORT QByteArray RestoreImages(QIODevice& input, const QString& folder, const QString& fileName, const std::shared_ptr<const ISettings>& settings = {});

using ExtractBookImagesCallback = std::function<bool(QString, QByteArray)>;
void ExtractBookImages(const QString& folder, const QString& fileName, const ExtractBookImagesCallback& callback, const std::shared_ptr<const ISettings>& settings = {});

}
