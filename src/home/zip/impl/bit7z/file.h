#pragma once

#include <memory>
#include <QDateTime>

class QString;

namespace bit7z {
class BitArchiveItemInfo;
class Bit7zLibrary;
}

namespace HomeCompa::ZipDetails {
class IFile;
}

namespace HomeCompa::ZipDetails::Impl::Bit7z {

struct FileItem
{
	size_t size;
	QDateTime time;
	uint32_t index;

	FileItem(const bit7z::BitArchiveItemInfo & info, const uint32_t index);
};

struct File
{
	static std::unique_ptr<IFile> Read(const bit7z::Bit7zLibrary & lib, std::istream & stream, const FileItem & item);
	static std::unique_ptr<IFile> Write(const bit7z::Bit7zLibrary & lib, std::ostream & stream, QString filename);
};

}
