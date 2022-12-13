#include <filesystem>
#include <fstream>
#include <sstream>

#include "zip/ZipArchive.h"

using namespace HomeCompa::Util;

int main([[maybe_unused]] int argc, [[maybe_unused]] char * argv [])
{
	assert(argc > 2);
	std::ofstream zipStream(std::filesystem::path(argv[1]), std::ios::binary | std::ios_base::trunc);

	ZipArchive zipArchive(zipStream);
	std::istringstream stream("test");
	zipArchive.Write(argv[2], stream);

	return 0;
}
