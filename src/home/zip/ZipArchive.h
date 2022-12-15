#pragma once

#include <iosfwd>

#include <string>
#include <vector>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "ZipLib.h"

namespace HomeCompa::Util {

struct ZipEntry
{
	std::string name;
	int64_t size;
	int64_t compressedSize;
};

using ZipEntries = std::vector<ZipEntry>;

class ZIP_API ZipArchive
{
	NON_COPY_MOVABLE(ZipArchive)

public:
	explicit ZipArchive(std::istream & stream);
	explicit ZipArchive(std::ostream & stream);
	~ZipArchive();

	[[nodiscard]] ZipEntries GetZipEntries() const;
	[[nodiscard]] std::istream & Read(std::string_view name);
	int64_t Write(std::string_view name, std::istream & stream);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
