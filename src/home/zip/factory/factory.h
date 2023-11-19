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
	static std::unique_ptr<IZip> Create(const QString & filename);
};

}
