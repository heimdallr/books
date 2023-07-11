#pragma once

#include "fnd/memory.h"
#include "interface/IUiFactory.h"

namespace Hypodermic {
class Container;
}

namespace HomeCompa::Flibrary {

class UiFactory : virtual public IUiFactory
{
public:
	explicit UiFactory(Hypodermic::Container & container);
	~UiFactory() override;

private: // IUiFactory
	std::shared_ptr<QWidget> CreateTreeViewWidget(TreeViewControllerType type) const override;

	std::shared_ptr<AbstractTreeViewController> GetTreeViewController() const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
