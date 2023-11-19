#include "factory.h"

#include <QFileInfo>

#include "fnd/FindPair.h"

#include "interface/error.h"
#include "interface/zip.h"
#include "impl/zip/zip.h"

using namespace HomeCompa::Zip;

namespace {

using Creator = std::unique_ptr<IZip>(*)(const QString & filename);
constexpr std::pair<const char *, Creator> CREATORS_BY_EXT[]
{
	{"zip", &Impl::Zip::Create},
};

constexpr std::pair<const char *, Creator> CREATORS_BY_SIGNATURE[]
{
	{"PK", &Impl::Zip::Create},
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
