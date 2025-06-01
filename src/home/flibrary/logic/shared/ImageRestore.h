#pragma once

#include <functional>
#include <memory>

#include "export/logic.h"

namespace HomeCompa
{
class ISettings;
}

class QIODevice;
class QByteArray;
class QString;

namespace HomeCompa
{
class Zip;
}

namespace HomeCompa::Flibrary
{

LOGIC_EXPORT QByteArray RestoreImages(QIODevice& input, const QString& folder, const QString& fileName, const std::shared_ptr<const ISettings>& settings = {});

using ExtractBookImagesCallback = std::function<bool(QString, QByteArray)>;
bool ExtractBookImages(const QString& folder, const QString& fileName, const ExtractBookImagesCallback& callback, const std::shared_ptr<const ISettings>& settings = {});

}
