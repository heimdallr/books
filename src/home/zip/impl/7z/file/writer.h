#pragma once
#include <memory>

class QIODevice;
class QString;
struct IOutArchive;

namespace HomeCompa::ZipDetails {
class IFile;
class ProgressCallback;
}

namespace HomeCompa::ZipDetails::Impl::SevenZip {
class Lib;

namespace File
{
std::unique_ptr<IFile> Write(IOutArchive & zip, QIODevice & oStream, QString filename, ProgressCallback & progress);
};

}
