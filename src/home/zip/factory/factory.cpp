#include "factory.h"

#include <QFileInfo>

#include "fnd/FindPair.h"

#include "zip/interface/error.h"
#include "zip/interface/zip.h"
#include "zip/impl/7z/archive.h"
#include "zip/impl/zip/archive.h"

using namespace HomeCompa::ZipDetails;

namespace {

using ReaderCreator = std::unique_ptr<IZip>(*)(const QString & filename, std::shared_ptr<ProgressCallback> progress);
constexpr std::pair<const char *, ReaderCreator> CREATORS_BY_EXT[]
{
	{"zip", &Impl::Zip::Archive::CreateReader},
	{"7z", &Impl::SevenZip::Archive::CreateReader},
};

constexpr std::pair<const char *, ReaderCreator> CREATORS_BY_SIGNATURE[]
{
	{"PK", &Impl::Zip::Archive::Archive::CreateReader},
	{"7z", &Impl::SevenZip::Archive::CreateReader},
};

using WriterCreator = std::unique_ptr<IZip>(*)(const QString & filename, std::shared_ptr<ProgressCallback> progress, bool appendMode);
constexpr std::pair<Factory::Format, WriterCreator> CREATORS_BY_FORMAT[]
{
	{Factory::Format::Zip, &Impl::Zip::Archive::CreateWriter},
	{Factory::Format::SevenZip, &Impl::SevenZip::Archive::CreateWriter},
};

using WriterCreatorStream = std::unique_ptr<IZip>(*)(QIODevice & stream, std::shared_ptr<ProgressCallback> progress, bool appendMode);
constexpr std::pair<Factory::Format, WriterCreatorStream> CREATORS_BY_FORMAT_STREAM[]
{
	{Factory::Format::Zip, &Impl::Zip::Archive::CreateWriterStream},
	{Factory::Format::SevenZip, &Impl::SevenZip::Archive::CreateWriterStream},
};

std::unique_ptr<IZip> CreateBySignature(const QString & filename, std::shared_ptr<ProgressCallback> progress)
{
	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly))
		Error::CannotOpenFile(filename);

	char buf[3];
	file.read(buf, 2);
	buf[2] = 0;

	return FindSecond(CREATORS_BY_SIGNATURE, buf, PszComparer {})(filename, std::move(progress));
}

}

std::unique_ptr<IZip> Factory::Create(const QString & filename, std::shared_ptr<ProgressCallback> progress)
{
	return FindSecond(CREATORS_BY_EXT, QFileInfo(filename).suffix().toStdString().data(), &CreateBySignature, PszComparer {})(filename, std::move(progress));
}

std::unique_ptr<IZip> Factory::Create(const QString & filename, std::shared_ptr<ProgressCallback> progress, const Format format, const bool appendMode)
{
	return FindSecond(CREATORS_BY_FORMAT, format)(filename, std::move(progress), appendMode);
}

std::unique_ptr<IZip> Factory::Create(QIODevice & stream, std::shared_ptr<ProgressCallback> progress, const Format format, const bool appendMode)
{
	return FindSecond(CREATORS_BY_FORMAT_STREAM, format)(stream, std::move(progress), appendMode);
}
