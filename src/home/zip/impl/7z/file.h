#pragma once

#include <memory>

class QString;
struct IInArchive;

namespace HomeCompa::Zip {
class IFile;
}

namespace HomeCompa::Zip::Impl::SevenZip {

struct File
{
	static std::unique_ptr<IFile> Read(CComPtr<IInArchive> zip, uint32_t index);
//	static std::unique_ptr<IFile> Write(QuaZip & zip, const QString & filename);
};

}
