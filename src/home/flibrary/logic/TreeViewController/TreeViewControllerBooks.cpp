#include "TreeViewControllerBooks.h"

#include <QModelIndex>
#include <QString>
#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "data/DataProvider.h"
#include "data/ModelProvider.h"
#include "database/DatabaseUser.h"
#include "interface/constants/Enums.h"
#include "interface/constants/ExportStat.h"
#include "interface/constants/ModelRole.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ILogicFactory.h"
#include "model/IModelObserver.h"
#include "shared/BooksContextMenuProvider.h"
#include "shared/ReaderController.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTEXT = "Books";

using ModelCreator = std::shared_ptr<QAbstractItemModel>(IModelProvider::*)(IDataItem::Ptr, IModelObserver &) const;

struct ModeDescriptor
{
	ViewMode viewMode;
	ModelCreator modelCreator;
};

constexpr std::pair<const char *, ModeDescriptor> MODE_NAMES[]
{
	{ QT_TRANSLATE_NOOP("Books", "List"), { ViewMode::List, &IModelProvider::CreateListModel} },
	{ QT_TRANSLATE_NOOP("Books", "Tree"), { ViewMode::Tree, &IModelProvider::CreateTreeModel} },
};

static_assert(std::size(MODE_NAMES) == static_cast<size_t>(ViewMode::Last));

auto GetViewModeImpl(const std::string & strMode)
{
	return FindSecond(MODE_NAMES, strMode.data(), MODE_NAMES[0].second, PszComparer {});
}

}

struct TreeViewControllerBooks::Impl final
	: IModelObserver
{
	ViewMode viewMode { ViewMode::Unknown };
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> model { std::shared_ptr<QAbstractItemModel>() };
	std::weak_ptr<const ILogicFactory> logicFactory;
	PropagateConstPtr<IAnnotationController, std::shared_ptr> annotationController;
	std::shared_ptr<const DatabaseUser> databaseUser;

	explicit Impl(std::weak_ptr<const ILogicFactory> logicFactory
		, std::shared_ptr<IAnnotationController> annotationController
		, std::shared_ptr<const DatabaseUser> databaseUser
	)
		: logicFactory(std::move(logicFactory))
		, annotationController(std::move(annotationController))
		, databaseUser(std::move(databaseUser))
	{
	}
};

TreeViewControllerBooks::TreeViewControllerBooks(std::shared_ptr<ISettings> settings
	, std::shared_ptr<DataProvider> dataProvider
	, const std::shared_ptr<const IModelProvider>& modelProvider
	, const std::shared_ptr<const ILogicFactory>& logicFactory
	, std::shared_ptr<IAnnotationController> annotationController
	, std::shared_ptr<DatabaseUser> databaseUser
)
	: AbstractTreeViewController(CONTEXT
		, std::move(settings)
		, std::move(dataProvider)
		, modelProvider
	)
	, m_impl(logicFactory
		, std::move(annotationController)
		, std::move(databaseUser)
	)
{
	Setup();

	m_dataProvider->SetBookRequestCallback([&] (IDataItem::Ptr data)
	{
		assert(m_impl->viewMode != ViewMode::Unknown);
		const auto invoker = MODE_NAMES[static_cast<int>(m_impl->viewMode)].second.modelCreator;
		m_impl->model.reset(((*IModelProvider::Lock(m_modelProvider)).*invoker)(std::move(data), *m_impl));
		Perform(&IObserver::OnModelChanged, m_impl->model.get());
	});

	PLOGD << "TreeViewControllerBooks created";
}

TreeViewControllerBooks::~TreeViewControllerBooks()
{
	PLOGD << "TreeViewControllerBooks destroyed";
}

std::vector<const char *> TreeViewControllerBooks::GetModeNames() const
{
	return GetModeNamesImpl(MODE_NAMES);
}

void TreeViewControllerBooks::SetCurrentId(QString id)
{
	m_impl->annotationController->SetCurrentBookId(std::move(id));
}

void TreeViewControllerBooks::OnModeChanged(const QString & mode)
{
	m_impl->viewMode = GetViewModeImpl(mode.toStdString()).viewMode;
	m_dataProvider->SetBooksViewMode(m_impl->viewMode);
	Perform(&IObserver::OnModeChanged, static_cast<int>(m_impl->viewMode));
}

int TreeViewControllerBooks::GetModeIndex(const QString & mode) const
{
	const auto enumMode = GetViewModeImpl(mode.toStdString()).viewMode;
	return static_cast<int>(enumMode);
}

ItemType TreeViewControllerBooks::GetItemType() const noexcept
{
	return ItemType::Books;
}

ViewMode TreeViewControllerBooks::GetViewMode() const noexcept
{
	return m_impl->viewMode;
}

void TreeViewControllerBooks::RequestContextMenu(const QModelIndex & index, const RequestContextMenuOptions options, RequestContextMenuCallback callback)
{
	auto menuProvider = ILogicFactory::Lock(m_impl->logicFactory)->CreateBooksContextMenuProvider();
	menuProvider->Request(index, options, [&, menuProvider, id = index.data(Role::Id).toString(), callback = std::move(callback)] (const IDataItem::Ptr & item) mutable
	{
		callback(id, item);
		menuProvider.reset();
	});
}

void TreeViewControllerBooks::OnContextMenuTriggered(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item)
{
	auto menuProvider = ILogicFactory::Lock(m_impl->logicFactory)->CreateBooksContextMenuProvider();
	menuProvider->OnContextMenuTriggered(model, index, indexList, std::move(item), [&, menuProvider, id = index.data(Role::Id).toString()] (const IDataItem::Ptr & itemResult) mutable
	{
		Perform(&IObserver::OnContextMenuTriggered, std::cref(id), std::cref(itemResult));
		menuProvider.reset();
	});
}

void TreeViewControllerBooks::OnDoubleClicked(const QModelIndex & index) const
{
	if (index.data(Role::Type).value<ItemType>() != ItemType::Books)
		return;

	auto readerController = ILogicFactory::Lock(m_impl->logicFactory)->CreateReaderController();
	readerController->Read(index.data(Role::Folder).toString(), index.data(Role::FileName).toString(), [this, readerController, id = index.data(Role::Id).toInt()]() mutable
	{
		try
		{
			const auto query = m_impl->databaseUser->Database()->CreateQuery(ExportStat::INSERT_QUERY);
			query->Bind(0, id);
			query->Bind(1, static_cast<int>(ExportStat::Type::Read));
			query->Execute();
		}
		catch(const std::exception & ex)
		{
			PLOGE << ex.what();
		}
		catch(...)
		{
			PLOGE << "Unknown error";
		}

		readerController.reset();
	});
}
