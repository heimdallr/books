#pragma once

#include <QString>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "common/Constant.h"

class QIODevice;

namespace HomeCompa::fb2cut {

constexpr auto MAX_SIZE = std::numeric_limits<int>::max();

inline QString GetCoverFileName(const QString & file, const QString & /*image*/)
{
	return file;
}
inline QString GetImageFileName(const QString & file, const QString & image)
{
	return QString("%1/%2").arg(file, image);
}

struct ImageSettings
{
	using ImageNameGetter = QString(*)(const QString & file, const QString & image);

	const char * type { nullptr };
	ImageNameGetter fileNameGetter { nullptr };

	QSize maxSize { MAX_SIZE, MAX_SIZE };
	int quality { -1 };
	bool save { true };
	bool grayscale { false };
};

struct Settings
{
	QStringList inputWildcards;
	ImageSettings cover { Global::COVER, &GetCoverFileName }, image { Global::IMAGE, &GetImageFileName };
	int maxThreadCount { static_cast<int>(std::thread::hardware_concurrency()) };
	int minImageFileSize { 1024 };
	bool saveFb2 { true };
	bool archiveFb2 { true };
	QDir dstDir;
	QString ffmpeg;
	QString archiver;
	QStringList archiverOptions { QStringList {}
		<< "a"
		<< "-mx9"
		<< "-sdel"
		<< "-m0=ppmd"
		<< "-ms=off"
		<< "-bt"
	};
	bool defaultArchiverOptions { true };
	int totalFileCount { 0 };
};

}
