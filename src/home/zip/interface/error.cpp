#include "error.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <qttranslation.h>

#include <plog/Log.h>

using namespace HomeCompa::Zip;

namespace {
constexpr auto CANNOT_OPEN_FILE = QT_TRANSLATE_NOOP("Error", "Cannot open file '%1'");
constexpr auto CANNOT_OPEN_ARCHIVE = QT_TRANSLATE_NOOP("Error", "Cannot open archive '%1'");
constexpr auto CANNOT_FIND = QT_TRANSLATE_NOOP("Error", "Cannot find file '%1'");
constexpr auto CANNOT_EXTRACT = QT_TRANSLATE_NOOP("Error", "Cannot extract file '%1'");

[[noreturn]] void Throw(const char * str, const QString & filename)
{
	PLOGE << QString(str).arg(filename);
	throw std::ios_base::failure(QCoreApplication::translate("Error", str).arg(QFileInfo(filename).fileName()).toStdString());
}

}

void Error::CannotOpenFile(const QString & filename)
{
	Throw(CANNOT_OPEN_FILE, filename);
}

void Error::CannotOpenArchive(const QString & filename)
{
	Throw(CANNOT_OPEN_ARCHIVE, filename);
}

void Error::CannotFindFileInArchive(const QString & filename)
{
	Throw(CANNOT_FIND, filename);
}

void Error::CannotExtractFileFromArchive(const QString & filename)
{
	Throw(CANNOT_EXTRACT, filename);
}
