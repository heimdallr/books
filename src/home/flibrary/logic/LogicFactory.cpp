#include "LogicFactory.h"

#include <Hypodermic/Hypodermic.h>
#include <plog/Log.h>

#include "util/executor/factory.h"
#include "util/ISettings.h"

#include "data/DataProvider.h"
#include "data/ModelProvider.h"

#include "interface/constants/Enums.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionController.h"
#include "interface/ui/IUiFactory.h"

#include "Annotation/ArchiveParser.h"

#include "TreeViewController/TreeViewControllerBooks.h"
#include "TreeViewController/TreeViewControllerNavigation.h"

#include "shared/BooksContextMenuProvider.h"
#include "shared/DatabaseController.h"
#include "shared/DatabaseUser.h"
#include "shared/GroupController.h"
#include "shared/ReaderController.h"

namespace HomeCompa::Flibrary {

struct LogicFactory::Impl final
{
	Hypodermic::Container & container;
	mutable std::shared_ptr<AbstractTreeViewController> controllers[static_cast<size_t>(ItemType::Last)];

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

std::shared_ptr<AbstractTreeViewController> LogicFactory::GetTreeViewController(const ItemType type) const
{
	auto & controller = m_impl->controllers[static_cast<size_t>(type)];
	if (!controller)
	{
		switch (type)
		{
			case ItemType::Books:
				controller = m_impl->container.resolve<TreeViewControllerBooks>();
				break;

			case ItemType::Navigation:
				controller = m_impl->container.resolve<TreeViewControllerNavigation>();
				break;

			default:
				return assert(false && "unexpected type"), nullptr;
		}
	}

	return controller;
}

std::shared_ptr<ArchiveParser> LogicFactory::CreateArchiveParser() const
{
	return m_impl->container.resolve<ArchiveParser>();
}

std::unique_ptr<Util::IExecutor> LogicFactory::GetExecutor(Util::ExecutorInitializer initializer) const
{
	return Util::ExecutorFactory::Create(Util::ExecutorImpl::Async, std::move(initializer));
}

std::shared_ptr<GroupController> LogicFactory::CreateGroupController() const
{
	return m_impl->container.resolve<GroupController>();
}

std::shared_ptr<BooksContextMenuProvider> LogicFactory::CreateBooksContextMenuProvider() const
{
	return m_impl->container.resolve<BooksContextMenuProvider>();
}

std::shared_ptr<ReaderController> LogicFactory::CreateReaderController() const
{
	return m_impl->container.resolve<ReaderController>();
}

}
