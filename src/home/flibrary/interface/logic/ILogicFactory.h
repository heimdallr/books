#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "FlIntLib.h"

namespace HomeCompa {

class ILogicFactory
{
public:
	FLINT_API static const ILogicFactory & Instance();

public:
	class FLINT_API Guard
	{
		NON_COPY_MOVABLE(Guard)

	public:
		explicit Guard(const ILogicFactory & logicFactory) noexcept;
		~Guard() noexcept;
	};

public:
	virtual ~ILogicFactory() = default;

	[[nodiscard]] virtual std::shared_ptr<class AbstractTreeViewController> CreateTreeViewController(enum class TreeViewControllerType type) const = 0;
};

}
