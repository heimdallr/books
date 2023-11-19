#pragma once

#include <memory>

#include "export/ZipFactory.h"

class QString;

namespace HomeCompa::Zip {
class IZip;
}

namespace HomeCompa::Zip {

struct ZIPFACTORY_EXPORT Factory
{
	enum class Format
	{
		Zip,
	};

	static std::unique_ptr<IZip> Create(const QString & filename);
	static std::unique_ptr<IZip> Create(const QString & filename, Format format);
};

}
