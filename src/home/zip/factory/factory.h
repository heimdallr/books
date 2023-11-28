#pragma once

#include <memory>

#include "export/ZipFactory.h"

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
	static std::unique_ptr<IZip> Create(const QString & filename, std::shared_ptr<ProgressCallback> progress, Format format);
};

}
