#pragma once

#include <memory>

#include <QStringList>

namespace HomeCompa::ZipDetails {

class IFile;

class IZip
{
public:
	virtual ~IZip() = default;
	[[nodiscard]] virtual QStringList GetFileNameList() const = 0;
	[[nodiscard]] virtual std::unique_ptr<IFile> Read(const QString& filename) const = 0;
	[[nodiscard]] virtual std::unique_ptr<IFile> Write(const QString & filename) = 0;
	[[nodiscard]] virtual size_t GetFileSize(const QString & filename) const = 0;
	[[nodiscard]] virtual const QDateTime & GetFileTime(const QString & filename) const = 0;
};

}
