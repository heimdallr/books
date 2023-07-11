#pragma once

#include "fnd/memory.h"
#include "interface/logic/ILogicFactory.h"

namespace Hypodermic {
class Container;
}

namespace HomeCompa::Flibrary {

class LogicFactory : virtual public ILogicFactory
{
public:
	explicit LogicFactory(Hypodermic::Container & container);
	~LogicFactory() override;

private: // ILogicFactory
	std::shared_ptr<AbstractTreeViewController> CreateTreeViewController(TreeViewControllerType type) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
