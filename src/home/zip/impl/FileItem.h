#pragma once

#include <QDir>

#include "zip/interface/error.h"

namespace HomeCompa::ZipDetails::SevenZip
{

struct FileItem
{
	uint32_t  index { 0 };
	QString   name;
	size_t    size { 0 };
	QDateTime time;
	bool      isDir { false };
};

struct FileStorage
{
	std::vector<FileItem>               files;
	std::unordered_map<QString, size_t> index;

	const FileItem& GetFile(const QString& name) const
	{
		const auto it = index.find(QDir::fromNativeSeparators(name));
		if (it == index.end())
			Error::CannotFindFileInArchive(name);

		return files[it->second];
	}
};

} // namespace HomeCompa::ZipDetails::SevenZip
