#include "factory.h"

#include <Hypodermic/Hypodermic.h>

#include "TreeViewController/TreeViewControllerBooks.h"
#include "TreeViewController/TreeViewControllerNavigation.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary {

struct LogicFactory::Impl
{
	Hypodermic::Container & container;

	Impl(Hypodermic::Container & container_)
		: container(container_)
	{
	}
};

LogicFactory::LogicFactory(Hypodermic::Container & container)
	: m_impl(container)
{
}

LogicFactory::~LogicFactory() = default;

std::shared_ptr<AbstractTreeViewController> LogicFactory::CreateTreeViewController(const TreeViewControllerType type) const
{
	switch (type)
	{
		case TreeViewControllerType::Books:
			return m_impl->container.resolve<TreeViewControllerBooks>();

		case TreeViewControllerType::Navigation:
			return m_impl->container.resolve<TreeViewControllerNavigation>();

		default:
			break;
	}

	return assert(false && "unexpected type"), nullptr;
}

}
