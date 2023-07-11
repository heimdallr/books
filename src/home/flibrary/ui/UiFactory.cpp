#include "UiFactory.h"

#include <Hypodermic/Hypodermic.h>

#include "interface/logic/ILogicFactory.h"
#include "interface/logic/ITreeViewController.h"

#include "TreeView.h"

namespace HomeCompa::Flibrary {

struct UiFactory::Impl
{
	Hypodermic::Container & container;

	Impl(Hypodermic::Container & container_)
		: container(container_)
	{
	}
};

UiFactory::UiFactory(Hypodermic::Container & container)
	: m_impl(container)
{
}

UiFactory::~UiFactory() = default;

std::shared_ptr<QWidget> UiFactory::CreateTreeViewWidget(const TreeViewControllerType type) const
{
	return std::make_shared<TreeView>(m_impl->container.resolve<ILogicFactory>()->CreateTreeViewController(type));
}

}
