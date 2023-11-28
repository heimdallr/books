#pragma once

#include <memory>

#include "export/ZipFactory.h"

class QString;

namespace HomeCompa::ZipDetails {
class IZip;
}

namespace HomeCompa::ZipDetails {

struct ZIPFACTORY_EXPORT Factory
{
	enum class Format
	{
		Zip,
		SevenZip,
	};

	static std::unique_ptr<IZip> Create(const QString & filename);
	static std::unique_ptr<IZip> Create(const QString & filename, Format format);
};

}
