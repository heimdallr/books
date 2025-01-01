#pragma once

#include <memory>

#include "export/ZipWrapper.h"

class QIODevice;
class QString;

namespace HomeCompa::ZipDetails {
class IZip;
class ProgressCallback;
}

namespace HomeCompa::ZipDetails::Impl::Zip {

struct Archive
{
	static ZIPWRAPPER_EXPORT std::unique_ptr<IZip> CreateReader(const QString & filename, std::shared_ptr<ProgressCallback> progress);
	static ZIPWRAPPER_EXPORT std::unique_ptr<IZip> CreateWriter(const QString & filename, std::shared_ptr<ProgressCallback> progress, bool appendMode);
	static ZIPWRAPPER_EXPORT std::unique_ptr<IZip> CreateWriterStream(QIODevice & stream, std::shared_ptr<ProgressCallback> progress, bool appendMode);
};

}
