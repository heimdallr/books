#pragma once

#include <memory>

class QuaZip;
class QString;

namespace HomeCompa::Zip {
class IFile;
}

namespace HomeCompa::Zip::Impl {

struct File
{
	static std::unique_ptr<IFile> Read(QuaZip & zip, const QString & filename);
};

}
