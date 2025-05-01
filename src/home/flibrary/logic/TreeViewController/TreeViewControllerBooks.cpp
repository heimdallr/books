#include "TreeViewControllerBooks.h"

#include <QModelIndex>
#include <QString>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/Enums.h"
#include "interface/constants/ExportStat.h"
#include "interface/constants/ModelRole.h"

#include "data/DataProvider.h"
#include "data/ModelProvider.h"
#include "model/IModelObserver.h"
#include "shared/BooksContextMenuProvider.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

namespace
{

constexpr auto CONTEXT = "Books";

using ModelCreator = std::shared_ptr<QAbstractItemModel> (IModelProvider::*)(IDataItem::Ptr, IModelObserver&, bool autoAcceptChildRows) const;

struct ModeDescriptor
{
	ViewMode viewMode;
	ModelCreator modelCreator;
};

constexpr std::pair<const char*, ModeDescriptor> MODE_NAMES[] {
	{ QT_TRANSLATE_NOOP("Books", "List"), { ViewMode::List, &IModelProvider::CreateListModel } },
	{ QT_TRANSLATE_NOOP("Books", "Tree"), { ViewMode::Tree, &IModelProvider::CreateTreeModel } },
};

static_assert(std::size(MODE_NAMES) == static_cast<size_t>(ViewMode::Last));

auto GetViewModeImpl(const std::string& strMode)
{
	return FindSecond(MODE_NAMES, strMode.data(), MODE_NAMES[0].second, PszComparer {});
}

}

struct TreeViewControllerBooks::Impl final : IModelObserver
{
	ViewMode viewMode { ViewMode::Unknown };
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> model { std::shared_ptr<QAbstractItemModel>() };
	std::weak_ptr<const ILogicFactory> logicFactory;
	PropagateConstPtr<IAnnotationController, std::shared_ptr> annotationController;
	std::shared_ptr<const IDatabaseUser> databaseUser;
	PropagateConstPtr<IBookInfoProvider, std::shared_ptr> dataProvider;
	std::shared_ptr<const IReaderController> readerController;

	Impl(std::weak_ptr<const ILogicFactory> logicFactory,
	     std::shared_ptr<IAnnotationController> annotationController,
	     std::shared_ptr<const IDatabaseUser> databaseUser,
	     std::shared_ptr<IBookInfoProvider> dataProvider,
	     std::shared_ptr<const IReaderController> readerController)
		: logicFactory { std::move(logicFactory) }
		, annotationController { std::move(annotationController) }
		, databaseUser { std::move(databaseUser) }
		, dataProvider { std::move(dataProvider) }
		, readerController { std::move(readerController) }
	{
	}
};

TreeViewControllerBooks::TreeViewControllerBooks(std::shared_ptr<ISettings> settings,
                                                 const std::shared_ptr<const IModelProvider>& modelProvider,
                                                 const std::shared_ptr<const ILogicFactory>& logicFactory,
                                                 std::shared_ptr<IBookInfoProvider> dataProvider,
                                                 std::shared_ptr<const IReaderController> readerController,
                                                 std::shared_ptr<IAnnotationController> annotationController,
                                                 std::shared_ptr<IDatabaseUser> databaseUser)
	: AbstractTreeViewController(CONTEXT, std::move(settings), modelProvider)
	, m_impl(logicFactory, std::move(annotationController), std::move(databaseUser), std::move(dataProvider), std::move(readerController))
{
	Setup();

	m_impl->dataProvider->SetBookRequestCallback(
		[&](IDataItem::Ptr data)
		{
			assert(m_impl->viewMode != ViewMode::Unknown);
			const auto invoker = MODE_NAMES[static_cast<int>(m_impl->viewMode)].second.modelCreator;
			auto model = std::invoke(invoker, IModelProvider::Lock(m_modelProvider), std::move(data), std::ref(*m_impl), true);
			m_impl->model.reset(std::move(model));
			Perform(&IObserver::OnModelChanged, m_impl->model.get());
		});

	PLOGV << "TreeViewControllerBooks created";
}

TreeViewControllerBooks::~TreeViewControllerBooks()
{
	PLOGV << "TreeViewControllerBooks destroyed";
}

std::vector<std::pair<const char*, int>> TreeViewControllerBooks::GetModeNames() const
{
	std::vector<std::pair<const char*, int>> result;
	std::ranges::transform(MODE_NAMES, std::back_inserter(result), [](const auto& item) { return std::make_pair(item.first, static_cast<int>(item.second.viewMode)); });
	return result;
}

void TreeViewControllerBooks::SetCurrentId(const ItemType type, QString id)
{
	m_impl->annotationController->SetCurrentBookId(type == ItemType::Books ? std::move(id) : QString {});
}

void TreeViewControllerBooks::OnModeChanged(const QString& mode)
{
	m_impl->viewMode = GetViewModeImpl(mode.toStdString()).viewMode;
	m_impl->dataProvider->SetBooksViewMode(m_impl->viewMode);
	Perform(&IObserver::OnModeChanged, static_cast<int>(m_impl->viewMode));
}

int TreeViewControllerBooks::GetModeIndex(const QString& mode) const
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

void TreeViewControllerBooks::RequestContextMenu(const QModelIndex& index, const RequestContextMenuOptions options, RequestContextMenuCallback callback)
{
	auto menuProvider = ILogicFactory::Lock(m_impl->logicFactory)->CreateBooksContextMenuProvider();
	menuProvider->Request(index,
	                      options,
	                      [&, menuProvider, id = index.data(Role::Id).toString(), callback = std::move(callback)](const IDataItem::Ptr& item) mutable
	                      {
							  callback(id, item);
							  menuProvider.reset();
						  });
}

void TreeViewControllerBooks::OnContextMenuTriggered(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item)
{
	auto menuProvider = ILogicFactory::Lock(m_impl->logicFactory)->CreateBooksContextMenuProvider();
	menuProvider->OnContextMenuTriggered(model,
	                                     index,
	                                     indexList,
	                                     std::move(item),
	                                     [&, menuProvider, id = index.data(Role::Id).toString()](const IDataItem::Ptr& itemResult) mutable
	                                     {
											 Perform(&IObserver::OnContextMenuTriggered, std::cref(id), std::cref(itemResult));
											 menuProvider.reset();
										 });
}

void TreeViewControllerBooks::OnDoubleClicked(const QModelIndex& index) const
{
	if (index.data(Role::Type).value<ItemType>() != ItemType::Books)
		return;

	m_impl->readerController->Read(index.data(Role::Folder).toString(),
	                               index.data(Role::FileName).toString(),
	                               [this, id = index.data(Role::Id).toInt()]
	                               {
									   try
									   {
										   const auto query = m_impl->databaseUser->Database()->CreateQuery(ExportStat::INSERT_QUERY);
										   query->Bind(0, id);
										   query->Bind(1, static_cast<int>(ExportStat::Type::Read));
										   query->Execute();
									   }
									   catch (const std::exception& ex)
									   {
										   PLOGE << ex.what();
									   }
									   catch (...)
									   {
										   PLOGE << "Unknown error";
									   }
								   });
}
