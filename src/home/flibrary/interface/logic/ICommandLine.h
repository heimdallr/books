#pragma once

#include <filesystem>

namespace HomeCompa::Flibrary {

class ICommandLine  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ICommandLine() = default;
	[[nodiscard]] virtual const std::filesystem::path & GetInpx() const noexcept = 0;
};

}
