#pragma once

#include <memory>

class QString;
struct IInArchive;

namespace HomeCompa::ZipDetails {
class IFile;
class ProgressCallback;
}

namespace HomeCompa::ZipDetails::Impl::SevenZip {

struct File
{
	static std::unique_ptr<IFile> Read(CComPtr<IInArchive> zip, uint32_t index, std::shared_ptr<ProgressCallback> progress);
//	static std::unique_ptr<IFile> Write(QuaZip & zip, const QString & filename, std::shared_ptr<ProgressCallback> progress);
};

}
