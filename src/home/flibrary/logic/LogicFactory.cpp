#include "LogicFactory.h"

#include <Hypodermic/Hypodermic.h>
#include <plog/Log.h>

#include "fnd/observable.h"

#include "data/DataProvider.h"
#include "data/ModelProvider.h"

#include "database/factory/Factory.h"
#include "database/interface/IDatabase.h"

#include "interface/constants/Enums.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionController.h"

#include "TreeViewController/TreeViewControllerBooks.h"
#include "TreeViewController/TreeViewControllerNavigation.h"

#include "logic/database/DatabaseController.h"
#include "util/executor/factory.h"
#include "util/IExecutor.h"
#include "util/ISettings.h"

namespace HomeCompa::Flibrary {

struct LogicFactory::Impl final
{
	Hypodermic::Container & container;
	std::unique_ptr<DatabaseController> databaseController;

	explicit Impl(Hypodermic::Container & container)
		: container(container)
	{
	}
};

LogicFactory::LogicFactory(Hypodermic::Container & container)
	: m_impl(container)
{
	m_impl->databaseController = std::make_unique<DatabaseController>(m_impl->container.resolve<ICollectionController>());

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

std::unique_ptr<DB::IDatabase> LogicFactory::GetDatabase() const
{
	return m_impl->databaseController->CreateDatabase();
}

std::unique_ptr<Util::IExecutor> LogicFactory::GetExecutor(Util::ExecutorInitializer initializer) const
{
	return Util::ExecutorFactory::Create(Util::ExecutorImpl::Async, std::move(initializer));
}

}
