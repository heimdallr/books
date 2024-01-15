#pragma once

#include <memory>

#include "export/7zWrapper.h"

class QString;

namespace HomeCompa::ZipDetails {
class IZip;
class ProgressCallback;
}

namespace HomeCompa::ZipDetails::Impl::SevenZip {

struct Archive
{
	static _7ZWRAPPER_EXPORT std::unique_ptr<IZip> CreateReader(const QString & filename, std::shared_ptr<ProgressCallback> progress);
	static _7ZWRAPPER_EXPORT std::unique_ptr<IZip> CreateWriter(const QString & filename, std::shared_ptr<ProgressCallback> progress, bool appendMode);
};

}
