#pragma once

#include <memory>

struct IInArchive;

namespace HomeCompa::ZipDetails
{
class IFile;
class ProgressCallback;
}

namespace HomeCompa::ZipDetails::SevenZip
{

struct FileItem;

namespace File
{
std::unique_ptr<IFile> Read(IInArchive& zip, const FileItem& fileItem, ProgressCallback& progress);
};

}
