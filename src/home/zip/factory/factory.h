#pragma once

#include <memory>

#include "export/ZipFactory.h"

class QIODevice;
class QString;

namespace HomeCompa::ZipDetails {
enum class Format;

class IZip;
class ProgressCallback;

struct ZIPFACTORY_EXPORT Factory
{
	static std::unique_ptr<IZip> Create(const QString & filename, std::shared_ptr<ProgressCallback> progress);
	static std::unique_ptr<IZip> Create(const QString & filename, std::shared_ptr<ProgressCallback> progress, Format format, bool appendMode);
	static std::unique_ptr<IZip> Create(QIODevice & stream, std::shared_ptr<ProgressCallback> progress, Format format, bool appendMode);
};

}
