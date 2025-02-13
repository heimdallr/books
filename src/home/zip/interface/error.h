#pragma once

class QString;

namespace HomeCompa::ZipDetails
{

struct Error
{
	[[noreturn]] static void CannotOpenFile(const QString& filename);
	[[noreturn]] static void CannotOpenArchive(const QString& filename);
	[[noreturn]] static void CannotFindFileInArchive(const QString& filename);
	[[noreturn]] static void CannotExtractFileFromArchive(const QString& filename);
	[[noreturn]] static void CannotCreateArchive(const QString& filename);
	[[noreturn]] static void CannotAddFileToArchive(const QString& filename);
	[[noreturn]] static void CannotLoadLibrary(const QString& libName);
	[[noreturn]] static void CannotFindEntryPoint(const QString& functionName);
	[[noreturn]] static void CannotCreateObject();
};

}
