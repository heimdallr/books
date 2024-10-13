#include "files.h"

#include <QDir>

namespace HomeCompa::Util {

QStringList ResolveWildcard(const QString & wildcard)
{
	const QFileInfo fileInfo(wildcard);
	const QDir dir(fileInfo.dir());
	const auto files = dir.entryList(QStringList() << fileInfo.fileName(), QDir::Files);
	QStringList result;
	result.reserve(files.size());
	std::ranges::transform(files, std::back_inserter(result), [&] (const auto & file)
	{
		return dir.filePath(file);
	});
	return result;
}

}
