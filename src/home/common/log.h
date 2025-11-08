#pragma once

#include <filesystem>

#include <QString>

#include <plog/Log.h>

namespace plog
{

inline Record& operator<<(Record& record, const std::filesystem::path& /*path*/)
{
	return record;
}

}
