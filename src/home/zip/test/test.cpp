#include <filesystem>
#include <fstream>
#include <sstream>

#include "zip/ZipArchive.h"

using namespace HomeCompa::Util;

int main([[maybe_unused]] int argc, [[maybe_unused]] char * argv [])
{
	assert(argc > 1);
	std::ifstream zipInputStream(std::filesystem::path(argv[1]), std::ios::binary);

	ZipArchive zipInputArchive(zipInputStream);
	const auto zipEntries = zipInputArchive.GetZipEntries();
	auto & decodedStream = zipInputArchive.Read(zipEntries[10].name);

	std::ofstream zipOutputStream("zip.zip", std::ios::binary);
	ZipArchive zipOutputArchive(zipOutputStream);
	zipOutputArchive.Write("file.fb2", decodedStream);

	return 0;
}
