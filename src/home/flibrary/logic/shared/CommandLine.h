#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/ICommandLine.h"

namespace HomeCompa::Flibrary {

class CommandLine final : virtual public ICommandLine
{
	NON_COPY_MOVABLE(CommandLine)

public:
	CommandLine();
	~CommandLine() override;

public:
	[[nodiscard]] const std::filesystem::path & GetInpxDir() const noexcept override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
