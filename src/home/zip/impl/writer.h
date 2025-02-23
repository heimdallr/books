#pragma once
#include <memory>
#include <vector>

#include "zip/interface/ProgressCallback.h"

class QIODevice;
class QString;
struct IOutArchive;

namespace HomeCompa::ZipDetails::SevenZip
{
struct FileStorage;
class Lib;

namespace File
{
bool Write(FileStorage& files, IOutArchive& zip, QIODevice& oStream, const std::vector<QString>& fileNames, const StreamGetter& streamGetter, const SizeGetter& sizeGetter, ProgressCallback& progress);
};

}
