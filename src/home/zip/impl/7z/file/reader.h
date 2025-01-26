#pragma once

#include <memory>
#include <unordered_map>

#include <QDateTime>

struct IInArchive;

namespace HomeCompa::ZipDetails {
class IFile;
class ProgressCallback;
}

namespace HomeCompa::ZipDetails::Impl::SevenZip {

struct FileItem
{
	uint32_t n;
	size_t size;
	QDateTime time;
};
using Files = std::unordered_map<QString, FileItem>;

namespace File
{
std::unique_ptr<IFile> Read(IInArchive & zip, const FileItem& fileItem, ProgressCallback & progress);
};

}
