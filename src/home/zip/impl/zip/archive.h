#pragma once

#include <memory>

#include "export/ZipWrapper.h"

class QString;

namespace HomeCompa::Zip {
class IZip;
}

namespace HomeCompa::Zip::Impl::Zip {

struct Archive
{
	static ZIPWRAPPER_EXPORT std::unique_ptr<IZip> CreateReader(const QString & filename);
	static ZIPWRAPPER_EXPORT std::unique_ptr<IZip> CreateWriter(const QString & filename);
};

}
