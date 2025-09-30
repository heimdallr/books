#pragma once

#include <memory>

#include <QStringList>

class QIODevice;
class QString;

namespace HomeCompa::ZipDetails
{
enum class Format;
class IZip;
class ProgressCallback;
}

namespace HomeCompa::ZipDetails::SevenZip
{

struct Archive
{
	static std::unique_ptr<IZip> CreateReader(const QString& filename, std::shared_ptr<ProgressCallback> progress);
	static std::unique_ptr<IZip> CreateReaderStream(QIODevice& stream, std::shared_ptr<ProgressCallback> progress);
	static std::unique_ptr<IZip> CreateWriter(const QString& filename, Format, std::shared_ptr<ProgressCallback> progress, bool appendMode);
	static std::unique_ptr<IZip> CreateWriterStream(QIODevice& stream, Format, std::shared_ptr<ProgressCallback> progress, bool appendMode);
	static bool                  IsArchive(const QString& filename);
	static QStringList           GetTypes();
};

}
