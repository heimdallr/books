#include "error.h"

#include <qttranslation.h>

#include <QCoreApplication>
#include <QFileInfo>

#include "log.h"

using namespace HomeCompa::ZipDetails;

namespace
{

constexpr auto CANNOT_OPEN_FILE           = QT_TRANSLATE_NOOP("Error", "Cannot open file '%1'");
constexpr auto CANNOT_OPEN_ARCHIVE        = QT_TRANSLATE_NOOP("Error", "Cannot open archive '%1'");
constexpr auto CANNOT_FIND                = QT_TRANSLATE_NOOP("Error", "Cannot find file '%1'");
constexpr auto CANNOT_EXTRACT             = QT_TRANSLATE_NOOP("Error", "Cannot extract file '%1'");
constexpr auto CANNOT_CREATE_ARCHIVE      = QT_TRANSLATE_NOOP("Error", "Cannot create file '%1'");
constexpr auto CANNOT_ADD_FILE_TO_ARCHIVE = QT_TRANSLATE_NOOP("Error", "Cannot add file '%1' to archive");
constexpr auto LOAD_LIBRARY_FAILED        = QT_TRANSLATE_NOOP("Error", "Cannot load %1");
constexpr auto GET_PROC_FAILED            = QT_TRANSLATE_NOOP("Error", "Cannon find entry point '%1'");
constexpr auto CANNOT_CREATE_OBJECT       = QT_TRANSLATE_NOOP("Error", "Cannot  create object");

[[noreturn]] void Throw(const char* str, const QString& filename = {})
{
	PLOGE << QString(str).arg(filename);
	throw std::ios_base::failure(QCoreApplication::translate("Error", str).arg(QFileInfo(filename).fileName()).toStdString());
}

}

void Error::CannotOpenFile(const QString& filename)
{
	Throw(CANNOT_OPEN_FILE, filename);
}

void Error::CannotOpenArchive(const QString& filename)
{
	Throw(CANNOT_OPEN_ARCHIVE, filename);
}

void Error::CannotFindFileInArchive(const QString& filename)
{
	Throw(CANNOT_FIND, filename);
}

void Error::CannotExtractFileFromArchive(const QString& filename)
{
	Throw(CANNOT_EXTRACT, filename);
}

void Error::CannotCreateArchive(const QString& filename)
{
	Throw(CANNOT_CREATE_ARCHIVE, filename);
}

void Error::CannotAddFileToArchive(const QString& filename)
{
	Throw(CANNOT_ADD_FILE_TO_ARCHIVE, filename);
}

void Error::CannotLoadLibrary(const QString& libName)
{
	Throw(LOAD_LIBRARY_FAILED, libName);
}

void Error::CannotFindEntryPoint(const QString& functionName)
{
	Throw(GET_PROC_FAILED, functionName);
}

void Error::CannotCreateObject()
{
	Throw(CANNOT_CREATE_OBJECT);
}
