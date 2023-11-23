#pragma once

#include <memory>

class QuaZip;
class QString;

namespace HomeCompa::Zip {
class IFile;
}

namespace HomeCompa::Zip::Impl::Zip {

struct File
{
	static std::unique_ptr<IFile> Read(QuaZip & zip, const QString & filename);
	static std::unique_ptr<IFile> Write(QuaZip & zip, const QString & filename);
};

}
