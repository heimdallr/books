#include "files.h"

#include <QCoreApplication>
#include <QDir>

namespace HomeCompa::Util
{

QStringList ResolveWildcard(const QString& wildcard)
{
	const QFileInfo fileInfo(wildcard);
	const QDir      dir(fileInfo.dir());
	const auto      files = dir.entryList(QStringList() << fileInfo.fileName(), QDir::Files);
	QStringList     result;
	result.reserve(files.size());
	std::ranges::transform(files, std::back_inserter(result), [&](const auto& file) {
		return QDir::fromNativeSeparators(dir.filePath(file));
	});
	return result;
}

QString ToRelativePath(const QString& path)
{
	return path.isEmpty() ? path : QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).relativeFilePath(path));
}

QString ToAbsolutePath(const QString& path)
{
	return path.isEmpty() ? path : QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(path));
}

} // namespace HomeCompa::Util
