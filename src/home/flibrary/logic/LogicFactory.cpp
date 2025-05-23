#include "LogicFactory.h"

#include <QTemporaryDir>

#include <Hypodermic/Hypodermic.h>

#include "interface/constants/Enums.h"
#include "interface/logic/IBookSearchController.h"
#include "interface/logic/ICollectionCleaner.h"
#include "interface/logic/IUpdateChecker.h"
#include "interface/logic/IUserDataController.h"

#include "Annotation/ArchiveParser.h"
#include "ChangeNavigationController/GroupController.h"
#include "TreeViewController/TreeViewControllerBooks.h"
#include "TreeViewController/TreeViewControllerNavigation.h"
#include "extract/BooksExtractor.h"
#include "extract/InpxGenerator.h"
#include "shared/BooksContextMenuProvider.h"
#include "shared/ZipProgressCallback.h"

#include "log.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

class QTemporaryDirWrapper : virtual public ILogicFactory::ITemporaryDir
{
private: // ILogicFactory::ITemporaryDir
	QString filePath(const QString& fileName) const override
	{
		return m_impl.filePath(fileName);
	}

	QString path() const override
	{
		return m_impl.path();
	}

private:
	QTemporaryDir m_impl;
};

class SingleTemporaryDir : virtual public ILogicFactory::ITemporaryDir
{
	NON_COPY_MOVABLE(SingleTemporaryDir)

public:
	SingleTemporaryDir() = default;

	~SingleTemporaryDir() override
	{
		m_impl.removeRecursively();
	}

private: // ILogicFactory::ITemporaryDir
	QString filePath(const QString& fileName) const override
	{
		if (!m_impl.exists() && !m_impl.mkpath("."))
			PLOGE << "Cannot create " << m_impl.path();

		return m_impl.filePath(fileName);
	}

	QString path() const override
	{
		return m_impl.path();
	}

private:
	QDir m_impl { QDir::tempPath() + "/" + PRODUCT_ID };
};

} // namespace

struct LogicFactory::Impl final
{
	Hypodermic::Container& container;
	std::shared_ptr<AbstractTreeViewController> controllers[static_cast<size_t>(ItemType::Last)];
	std::vector<std::shared_ptr<ITemporaryDir>> temporaryDirs;
	std::shared_ptr<ITemporaryDir> singleTemporaryDir;

	std::mutex progressControllerGuard;
	std::shared_ptr<IProgressController> progressController;

	explicit Impl(Hypodermic::Container& container)
		: container(container)
	{
	}
};

LogicFactory::LogicFactory(Hypodermic::Container& container)
	: m_impl { std::make_unique<Impl>(container) }
{
	PLOGV << "LogicFactory created";
}

LogicFactory::~LogicFactory()
{
	PLOGV << "LogicFactory destroyed";
}

std::shared_ptr<ITreeViewController> LogicFactory::GetTreeViewController(const ItemType type) const
{
	auto& controller = m_impl->controllers[static_cast<size_t>(type)];
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

std::shared_ptr<IBookSearchController> LogicFactory::CreateSearchController() const
{
	return m_impl->container.resolve<IBookSearchController>();
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

std::shared_ptr<IInpxGenerator> LogicFactory::CreateInpxGenerator() const
{
	return m_impl->container.resolve<InpxGenerator>();
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

std::shared_ptr<ILogicFactory::ITemporaryDir> LogicFactory::CreateTemporaryDir(const bool singleInstance) const
{
	if (!singleInstance)
		return m_impl->temporaryDirs.emplace_back(std::make_shared<QTemporaryDirWrapper>());

	if (!m_impl->singleTemporaryDir)
		m_impl->singleTemporaryDir = std::make_shared<SingleTemporaryDir>();

	return m_impl->singleTemporaryDir;
}

std::shared_ptr<IProgressController> LogicFactory::GetProgressController() const
{
	auto result = std::move(m_impl->progressController);
	m_impl->progressControllerGuard.unlock();
	return result;
}
