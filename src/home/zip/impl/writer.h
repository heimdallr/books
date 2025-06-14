#pragma once
#include <memory>
#include <vector>

#include "zip/interface/ProgressCallback.h"

class QIODevice;
class QString;
struct IOutArchive;

namespace HomeCompa
{
class IZipFileProvider;
}

namespace HomeCompa::ZipDetails::SevenZip
{
struct FileStorage;
class Lib;

namespace File
{
bool Write(FileStorage& files, IOutArchive& zip, QIODevice& oStream, std::shared_ptr<IZipFileProvider> zipFileProvider, ProgressCallback& progress);
};

}
