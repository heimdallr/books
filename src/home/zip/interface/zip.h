#pragma once

#include <memory>
#include <QStringList>

namespace HomeCompa::Zip {

class IFile;

class IZip
{
public:
	virtual ~IZip() = default;
	[[nodiscard]] virtual QStringList GetFileNameList() const = 0;
	[[nodiscard]] virtual std::unique_ptr<IFile> Read(const QString& filename) const = 0;
	[[nodiscard]] virtual std::unique_ptr<IFile> Write(const QString & filename) = 0;
};

}
