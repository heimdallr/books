#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/logic/ILogicFactory.h"

namespace Hypodermic {
class Container;
}

namespace HomeCompa::Flibrary {

class LogicFactory final : virtual public ILogicFactory
{
	NON_COPY_MOVABLE(LogicFactory)

public:
	explicit LogicFactory(Hypodermic::Container & container);
	~LogicFactory() override;

private: // ILogicFactory
	[[nodiscard]] std::shared_ptr<AbstractTreeViewController> CreateTreeViewController(ItemType type) const override;
	[[nodiscard]] std::unique_ptr<Util::IExecutor> GetExecutor(Util::ExecutorInitializer initializer) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
