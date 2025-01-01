#pragma once

#include <memory>

#include "export/ZipFactory.h"

class QIODevice;
class QString;

namespace HomeCompa::ZipDetails {

class IZip;
class ProgressCallback;

struct ZIPFACTORY_EXPORT Factory
{
	enum class Format
	{
		Zip,
		SevenZip,
	};

	static std::unique_ptr<IZip> Create(const QString & filename, std::shared_ptr<ProgressCallback> progress);
	static std::unique_ptr<IZip> Create(const QString & filename, std::shared_ptr<ProgressCallback> progress, Format format, bool appendMode);
	static std::unique_ptr<IZip> Create(QIODevice & stream, std::shared_ptr<ProgressCallback> progress, Format format, bool appendMode);
};

}
