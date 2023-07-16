#include "LogicFactory.h"

#include <Hypodermic/Hypodermic.h>

#include "fnd/observable.h"

#include "database/factory/Factory.h"
#include "database/interface/IDatabase.h"

#include "interface/logic/ICollectionController.h"

#include "TreeViewController/TreeViewControllerBooks.h"
#include "TreeViewController/TreeViewControllerNavigation.h"


#include "logic/database/DatabaseController.h"
#include "util/ISettings.h"

namespace HomeCompa::Flibrary {

struct LogicFactory::Impl final
	: Observable<IObserver>
{
	Hypodermic::Container & container;
	std::shared_ptr<DB::IDatabase> database;
	std::unique_ptr<DatabaseController> databaseController;

	explicit Impl(Hypodermic::Container & container)
		: container(container)
	{
	}
};

LogicFactory::LogicFactory(Hypodermic::Container & container)
	: m_impl(container)
{
	m_impl->databaseController = std::make_unique<DatabaseController>(*this, m_impl->container.resolve<ICollectionController>());
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
	}

	return assert(false && "unexpected type"), nullptr;
}

void LogicFactory::SetDatabase(std::shared_ptr<DB::IDatabase> db)
{
	m_impl->database = std::move(db);
	m_impl->Perform(&IObserver::OnDatabaseChanged, m_impl->database);
}

std::shared_ptr<DB::IDatabase> LogicFactory::GetDatabase() const
{
	return m_impl->database;
}

void LogicFactory::RegisterObserver(IObserver * observer)
{
	m_impl->Register(observer);
}

void LogicFactory::UnregisterObserver(IObserver * observer)
{
	m_impl->Unregister(observer);
}

}
