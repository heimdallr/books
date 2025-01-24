#pragma once

#include <memory>

#include "export/bit7zWrapper.h"

class QIODevice;
class QString;

namespace HomeCompa::ZipDetails {
class IZip;
class ProgressCallback;
}

namespace HomeCompa::ZipDetails::Impl::Bit7z {

struct Archive
{
	static BIT7ZWRAPPER_EXPORT std::unique_ptr<IZip> CreateReader(const QString & filename, std::shared_ptr<ProgressCallback> progress);
	static BIT7ZWRAPPER_EXPORT std::unique_ptr<IZip> CreateWriter(const QString & filename, std::shared_ptr<ProgressCallback> progress, bool appendMode);
	static BIT7ZWRAPPER_EXPORT std::unique_ptr<IZip> CreateWriterStream(QIODevice & stream, std::shared_ptr<ProgressCallback> progress, bool appendMode);
};

}
