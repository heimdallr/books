#include "LogicFactory.h"

#include <QTemporaryDir>

#include <Hypodermic/Hypodermic.h>
#include <plog/Log.h>

#include "util/executor/factory.h"
#include "util/ISettings.h"

#include "data/DataProvider.h"
#include "data/ModelProvider.h"

#include "interface/constants/Enums.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionCleaner.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseController.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/INavigationQueryExecutor.h"
#include "interface/logic/IProgressController.h"
#include "interface/logic/IReaderController.h"
#include "interface/logic/IScriptController.h"
#include "interface/logic/IUpdateChecker.h"
#include "interface/logic/IUserDataController.h"
#include "interface/ui/IUiFactory.h"

#include "Annotation/ArchiveParser.h"

#include "TreeViewController/TreeViewControllerBooks.h"
#include "TreeViewController/TreeViewControllerNavigation.h"

#include "ChangeNavigationController/GroupController.h"
#include "ChangeNavigationController/SearchController.h"

#include "extract/BooksExtractor.h"
#include "extract/InpxCollectionExtractor.h"

#include "shared/BooksContextMenuProvider.h"
#include "shared/ZipProgressCallback.h"

using namespace HomeCompa;
using namespace Flibrary;

struct LogicFactory::Impl final
{
	Hypodermic::Container & container;
	std::shared_ptr<AbstractTreeViewController> controllers[static_cast<size_t>(ItemType::Last)];
	std::vector<std::shared_ptr<QTemporaryDir>> temporaryDirs;

	std::mutex progressControllerGuard;
	std::shared_ptr<IProgressController> progressController;

	explicit Impl(Hypodermic::Container & container)
		: container(container)
	{
	}
};

LogicFactory::LogicFactory(Hypodermic::Container & container)
	: m_impl { std::make_unique<Impl>(container) }
{
	PLOGD << "LogicFactory created";
}

LogicFactory::~LogicFactory()
{
	PLOGD << "LogicFactory destroyed";
}

std::shared_ptr<ITreeViewController> LogicFactory::GetTreeViewController(const ItemType type) const
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

std::shared_ptr<SearchController> LogicFactory::CreateSearchController() const
{
	return m_impl->container.resolve<SearchController>();
}

std::shared_ptr<BooksContextMenuProvider> LogicFactory::CreateBooksContextMenuProvider() const
{
	return m_impl->container.resolve<BooksContextMenuProvider>();
}

std::shared_ptr<IUserDataController> LogicFactory::CreateUserDataController() const
{
	return m_impl->container.resolve<IUserDataController>();
}

std::shared_ptr<BooksExtractor> LogicFactory::CreateBooksExtractor() const
{
	return m_impl->container.resolve<BooksExtractor>();
}

std::shared_ptr<InpxCollectionExtractor> LogicFactory::CreateInpxCollectionExtractor() const
{
	return m_impl->container.resolve<InpxCollectionExtractor>();
}

std::shared_ptr<IUpdateChecker> LogicFactory::CreateUpdateChecker() const
{
	return m_impl->container.resolve<IUpdateChecker>();
}

std::shared_ptr<ICollectionCleaner> LogicFactory::CreateCollectionCleaner() const
{
	return m_impl->container.resolve<ICollectionCleaner>();
}

std::shared_ptr<Zip::ProgressCallback> LogicFactory::CreateZipProgressCallback(std::shared_ptr<IProgressController> progressController) const
{
	m_impl->progressControllerGuard.lock();
	m_impl->progressController = std::move(progressController);
	return m_impl->container.resolve<ZipProgressCallback>();
}

std::shared_ptr<QTemporaryDir> LogicFactory::CreateTemporaryDir() const
{
	auto temporaryDir = std::make_shared<QTemporaryDir>();
	m_impl->temporaryDirs.push_back(temporaryDir);
	return temporaryDir;
}

std::shared_ptr<IProgressController> LogicFactory::GetProgressController() const
{
	auto result = std::move(m_impl->progressController);
	m_impl->progressControllerGuard.unlock();
	return result;
}
