#pragma once

#include "export/ZipInterface.h"

class QString;

namespace HomeCompa::Zip {

struct ZIPINTERFACE_EXPORT Error
{
	[[noreturn]] static void CannotOpenFile(const QString & filename);
	[[noreturn]] static void CannotOpenArchive(const QString & filename);
	[[noreturn]] static void CannotFindFileInArchive(const QString & filename);
	[[noreturn]] static void CannotExtractFileFromArchive(const QString & filename);
};

}
