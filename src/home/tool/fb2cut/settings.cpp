#include "settings.h"

#include <ostream>

#include "zip.h"

std::ostream& operator<<(std::ostream& stream, const HomeCompa::fb2cut::ImageSettings& settings)
{
	return stream << std::endl
	              << "max size: " << settings.maxSize.width() << "x" << settings.maxSize.height() << std::endl
	              << "compression quality: " << settings.quality << std::endl
	              << "grayscale: " << (settings.grayscale ? "on" : "off");
}

std::ostream& operator<<(std::ostream& stream, const HomeCompa::fb2cut::Settings& settings)
{
	if (settings.cover.save)
		stream << std::endl << "covers settings: " << settings.cover;
	else
		stream << std::endl << "covers skipped";

	if (settings.image.save)
		stream << std::endl << "images settings: " << settings.image;
	else
		stream << std::endl << "images skipped";

	if (!settings.saveFb2)
		stream << std::endl << "fb2 skipped";
	else
		stream << std::endl << "fb2 archiving " << (settings.archiveFb2 ? "enabled" : "disabled");

	if (!settings.imageStatistics.isEmpty())
		stream << std::endl << settings.imageStatistics.toStdString();

	stream << std::endl << "output format: " << settings.format;

	if (!settings.ffmpeg.isEmpty())
		stream << std::endl << "ffmpeg: " << settings.ffmpeg.toStdString();

	return stream << std::endl << "max thread count: " << settings.maxThreadCount << std::endl << "min image file size: " << settings.minImageFileSize;
}
