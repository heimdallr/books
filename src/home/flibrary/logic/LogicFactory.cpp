#include "LogicFactory.h"

#include <Hypodermic/Hypodermic.h>
#include <plog/Log.h>

#include "util/executor/factory.h"
#include "util/ISettings.h"

#include "data/DataProvider.h"
#include "data/ModelProvider.h"

#include "database/DatabaseController.h"

#include "interface/constants/Enums.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionController.h"

#include "TreeViewController/TreeViewControllerBooks.h"
#include "TreeViewController/TreeViewControllerNavigation.h"

#include "shared/DatabaseUser.h"

namespace HomeCompa::Flibrary {

struct LogicFactory::Impl final
{
	Hypodermic::Container & container;

	explicit Impl(Hypodermic::Container & container)
		: container(container)
	{
	}
};

LogicFactory::LogicFactory(Hypodermic::Container & container)
	: m_impl(container)
{
	PLOGD << "LogicFactory created";
}

LogicFactory::~LogicFactory()
{
	PLOGD << "LogicFactory destroyed";
}

std::shared_ptr<AbstractTreeViewController> LogicFactory::CreateTreeViewController(const ItemType type) const
{
	switch (type)
	{
		case ItemType::Books:
			return m_impl->container.resolve<TreeViewControllerBooks>();

		case ItemType::Navigation:
			return m_impl->container.resolve<TreeViewControllerNavigation>();

		default:
			break;
	}

	return assert(false && "unexpected type"), nullptr;
}

std::unique_ptr<Util::IExecutor> LogicFactory::GetExecutor(Util::ExecutorInitializer initializer) const
{
	return Util::ExecutorFactory::Create(Util::ExecutorImpl::Async, std::move(initializer));
}

}
