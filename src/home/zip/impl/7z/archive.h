#pragma once

#include <memory>

#include "export/7zWrapper.h"

class QString;

namespace HomeCompa::Zip {
class IZip;
}

namespace HomeCompa::Zip::Impl::SevenZip {

struct Archive
{
	static _7ZWRAPPER_EXPORT std::unique_ptr<IZip> CreateReader(const QString & filename);
	static _7ZWRAPPER_EXPORT std::unique_ptr<IZip> CreateWriter(const QString & filename);
};

}
