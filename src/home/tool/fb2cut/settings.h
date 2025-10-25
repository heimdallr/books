#pragma once

#include <QDir>
#include <QSize>
#include <QString>

#include "common/Constant.h"

#include "zip.h"

namespace HomeCompa::fb2cut
{

constexpr auto MAX_SIZE = std::numeric_limits<int>::max();

inline QString GetCoverFileName(const QString& file, const QString& /*image*/)
{
	return file;
}

inline QString GetImageFileName(const QString& file, const QString& image)
{
	return QString("%1/%2").arg(file, image);
}

struct ImageSettings
{
	using ImageNameGetter = QString (*)(const QString& file, const QString& image);

	const char*     type { nullptr };
	ImageNameGetter fileNameGetter { nullptr };

	QSize maxSize { MAX_SIZE, MAX_SIZE };
	int   quality { -1 };
	bool  save { true };
	bool  grayscale { false };

	bool operator==(const ImageSettings& rhs) const noexcept
	{
		return maxSize == rhs.maxSize && quality == rhs.quality && save == rhs.save && grayscale == rhs.grayscale;
	}
};

struct Settings
{
	QStringList   inputWildcards;
	ImageSettings cover { Global::COVER, &GetCoverFileName }, image { Global::IMAGE, &GetImageFileName };
	int           maxThreadCount { static_cast<int>(std::thread::hardware_concurrency()) };
	int           minImageFileSize { 1024 };
	bool          saveFb2 { true };
	bool          archiveFb2 { true };
	QDir          dstDir;
	QString       ffmpeg;
	QString       imageStatistics;
	bool          needHash { false };
	QString       skipFileName;
	QString       archiver;
	QString       archiverOptions;
	int           totalFileCount { 0 };
	Zip::Format   format { Zip::Format::SevenZip };
};

} // namespace HomeCompa::fb2cut

std::ostream& operator<<(std::ostream& stream, const HomeCompa::fb2cut::ImageSettings& settings);
std::ostream& operator<<(std::ostream& stream, const HomeCompa::fb2cut::Settings& settings);
