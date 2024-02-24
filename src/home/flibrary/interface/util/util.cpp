#include "util.h"

#include <QCoreApplication>
#include <QFile>

namespace HomeCompa::Flibrary {

bool IsPortable()
{
	const auto fileNamePortable = QString("%1/portable").arg(QCoreApplication::applicationDirPath());
	return QFile::exists(fileNamePortable);
}

}
