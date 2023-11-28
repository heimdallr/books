#include "factory.h"

#include <QFileInfo>

#include "fnd/FindPair.h"

#include "zip/interface/error.h"
#include "zip/interface/zip.h"
#include "zip/impl/zip/archive.h"
#include "zip/impl/7z/archive.h"

using namespace HomeCompa::ZipDetails;

namespace {

using Creator = std::unique_ptr<IZip>(*)(const QString & filename);
constexpr std::pair<const char *, Creator> CREATORS_BY_EXT[]
{
	{"zip", &Impl::Zip::Archive::CreateReader},
	{"7z", &Impl::SevenZip::Archive::CreateReader},
};

constexpr std::pair<const char *, Creator> CREATORS_BY_SIGNATURE[]
{
	{"PK", &Impl::Zip::Archive::Archive::CreateReader},
	{"7z", &Impl::SevenZip::Archive::CreateReader},
};

constexpr std::pair<Factory::Format, Creator> CREATORS_BY_FORMAT[]
{
	{Factory::Format::Zip, &Impl::Zip::Archive::CreateWriter},
	{Factory::Format::SevenZip, &Impl::SevenZip::Archive::CreateWriter},
};

std::unique_ptr<IZip> CreateBySignature(const QString & filename)
{
	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly))
		Error::CannotOpenFile(filename);

	char buf[3];
	file.read(buf, 2);
	buf[2] = 0;

	return FindSecond(CREATORS_BY_SIGNATURE, buf, PszComparer {})(filename);
}

}

std::unique_ptr<IZip> Factory::Create(const QString & filename)
{
	return FindSecond(CREATORS_BY_EXT, QFileInfo(filename).suffix().toStdString().data(), &CreateBySignature, PszComparer {})(filename);
}

std::unique_ptr<IZip> Factory::Create(const QString & filename, const Format format)
{
	return FindSecond(CREATORS_BY_FORMAT, format)(filename);
}
