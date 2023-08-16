#include "BooksContextMenuProvider.h"

#include <QModelIndex>
#include <QString>
#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "database/interface/ITransaction.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"

#include "interface/ui/IUiFactory.h"

#include "data/DataItem.h"
#include "data/DataProvider.h"
#include "shared/ReaderController.h"

#include "BooksExtractor.h"
#include "DatabaseUser.h"
#include "GroupController.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto CONTEXT = "BookContextMenu";
constexpr auto READ_BOOK = QT_TRANSLATE_NOOP("BookContextMenu", "Read");
constexpr auto GROUPS = QT_TRANSLATE_NOOP("BookContextMenu", "Groups");
constexpr auto GROUPS_ADD_TO_NEW = QT_TRANSLATE_NOOP("BookContextMenu", "New group...");
constexpr auto GROUPS_ADD_TO = QT_TRANSLATE_NOOP("BookContextMenu", "Add to");
constexpr auto GROUPS_REMOVE_FROM = QT_TRANSLATE_NOOP("BookContextMenu", "Remove from");
constexpr auto GROUPS_REMOVE_FROM_ALL = QT_TRANSLATE_NOOP("BookContextMenu", "All");
constexpr auto REMOVE_BOOK = QT_TRANSLATE_NOOP("BookContextMenu", "Remove book");
constexpr auto REMOVE_BOOK_UNDO = QT_TRANSLATE_NOOP("BookContextMenu", "Undo book deletion");
constexpr auto SEND_TO = QT_TRANSLATE_NOOP("BookContextMenu", "Send to device");
constexpr auto SEND_AS_ARCHIVE = QT_TRANSLATE_NOOP("BookContextMenu", "In zip archive");
constexpr auto SEND_AS_IS = QT_TRANSLATE_NOOP("BookContextMenu", "In original format");
constexpr auto SELECT_SEND_TO_FOLDER = QT_TRANSLATE_NOOP("BookContextMenu", "Select destination folder");
TR_DEF

constexpr auto GROUPS_QUERY = "select g.GroupID, g.Title, coalesce(gl.BookID, -1) from Groups_User g left join Groups_List_User gl on gl.GroupID = g.GroupID and gl.BookID = ?";

using GroupActionFunction = void (GroupController::*)(GroupController::Id id, GroupController::Ids ids, GroupController::Callback callback) const;

#define MENU_ACTION_ITEMS_X_MACRO     \
MENU_ACTION_ITEM(ReadBook)            \
MENU_ACTION_ITEM(RemoveBook)          \
MENU_ACTION_ITEM(UndoRemoveBook)      \
MENU_ACTION_ITEM(AddToNewGroup)       \
MENU_ACTION_ITEM(AddToGroup)          \
MENU_ACTION_ITEM(RemoveFromGroup)     \
MENU_ACTION_ITEM(RemoveFromAllGroups) \
MENU_ACTION_ITEM(SendAsArchive)       \
MENU_ACTION_ITEM(SendAsIs)

enum class MenuAction
{
	None = -1,
#define MENU_ACTION_ITEM(NAME) NAME,
		MENU_ACTION_ITEMS_X_MACRO
#undef	MENU_ACTION_ITEM
};

class IContextMenuHandler  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Function = void (IContextMenuHandler::*)(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, BooksContextMenuProvider::Callback callback) const;

public:
	virtual ~IContextMenuHandler() = default;

#define MENU_ACTION_ITEM(NAME) virtual void NAME(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, BooksContextMenuProvider::Callback callback) const = 0;
		MENU_ACTION_ITEMS_X_MACRO
#undef	MENU_ACTION_ITEM
};

constexpr std::pair<MenuAction, IContextMenuHandler::Function> MENU_HANDLERS[]
{
	{ MenuAction::AddToGroup, &IContextMenuHandler::AddToGroup },
#define MENU_ACTION_ITEM(NAME) { MenuAction::NAME, &IContextMenuHandler::NAME },
		MENU_ACTION_ITEMS_X_MACRO
#undef	MENU_ACTION_ITEM
};

IDataItem::Ptr & Add(const IDataItem::Ptr & dst, QString title = {}, const MenuAction id = MenuAction::None)
{
	auto item = MenuItem::Create();
	item->SetData(std::move(title), MenuItem::Column::Title);
	item->SetData(QString::number(static_cast<int>(id)), MenuItem::Column::Id);
	return dst->AppendChild(std::move(item));
}

void CreateGroupMenu(const IDataItem::Ptr & parent, const QString & id, DB::IDatabase & db)
{
	const auto add = Add(parent, Tr(GROUPS_ADD_TO), MenuAction::AddToGroup);
	const auto remove = Add(parent, Tr(GROUPS_REMOVE_FROM), MenuAction::RemoveFromGroup);

	const auto query = db.CreateQuery(GROUPS_QUERY);
	query->Bind(0, id.toInt());
	for (query->Execute(); !query->Eof(); query->Next())
	{
		const auto needAdd = query->Get<long long>(2) < 0;
		const auto & item = needAdd ? add : remove;
		Add(item, query->Get<const char *>(1), needAdd ? MenuAction::AddToGroup : MenuAction::RemoveFromGroup)
			->SetData(QString::number(query->Get<long long>(0)), MenuItem::Column::Parameter);
	}

	if (remove->GetChildCount() > 0)
	{
		Add(remove);
		Add(remove, Tr(GROUPS_REMOVE_FROM_ALL), MenuAction::RemoveFromAllGroups)
			->SetData(QString::number(-1), MenuItem::Column::Parameter);
	}
	else
	{
		Add(remove);
		remove->SetData(QVariant(false).toString(), MenuItem::Column::Enabled);
	}

	if (add->GetChildCount() > 0)
		Add(add);

	Add(add, Tr(GROUPS_ADD_TO_NEW), MenuAction::AddToNewGroup)
		->SetData(QString::number(-1), MenuItem::Column::Parameter);
}

}

class BooksContextMenuProvider::Impl final
	: public IContextMenuHandler
{
public:
	explicit Impl(std::shared_ptr<DatabaseUser> databaseUser
		, std::shared_ptr<ILogicFactory> logicFactory
		, std::shared_ptr<IUiFactory> uiFactory
		, std::shared_ptr<GroupController> groupController
		, std::shared_ptr<DataProvider> dataProvider
	)
		: m_databaseUser(std::move(databaseUser))
		, m_logicFactory(std::move(logicFactory))
		, m_uiFactory(std::move(uiFactory))
		, m_groupController(std::move(groupController))
		, m_dataProvider(std::move(dataProvider))
	{
	}

public:
	void Request(const QModelIndex & index, Callback callback)
	{
		m_databaseUser->Execute({ "Create context menu", [
			  id = index.data(Role::Id).toString()
			, type = index.data(Role::Type).value<ItemType>()
			, removed = index.data(Role::IsRemoved).toBool()
			, callback = std::move(callback)
			, db = m_databaseUser->Database()
		]() mutable
		{
			auto result = MenuItem::Create();

			if (type == ItemType::Books)
				Add(result, Tr(READ_BOOK), MenuAction::ReadBook);

			{
				const auto & send = Add(result, Tr(SEND_TO));
				Add(send, Tr(SEND_AS_ARCHIVE), MenuAction::SendAsArchive);
				Add(send, Tr(SEND_AS_IS), MenuAction::SendAsIs);
			}

			if (type == ItemType::Books)
				CreateGroupMenu(Add(result, Tr(GROUPS), MenuAction::None), id, *db);

			if (type == ItemType::Books)
				Add(result, Tr(removed ? REMOVE_BOOK_UNDO : REMOVE_BOOK), removed ? MenuAction::UndoRemoveBook : MenuAction::RemoveBook);

			return [callback = std::move(callback), result = std::move(result)] (size_t)
			{
				if (result->GetChildCount() > 0)
					callback(result);
			};
		} });
	}

private: // IContextMenuHandler
	void ReadBook(QAbstractItemModel * /*model*/, const QModelIndex & index, const QList<QModelIndex> & /*indexList*/, IDataItem::Ptr item, Callback callback) const override
	{
		auto readerController = m_logicFactory->CreateReaderController();
		readerController->Read(index.data(Role::Folder).toString(), index.data(Role::FileName).toString(), [readerController, item = std::move(item), callback = std::move(callback)] () mutable
		{
			callback(item);
			readerController.reset();
		});
	}

	void RemoveBook(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		ChangeBookRemoved(model, index, indexList, std::move(item), std::move(callback), true);
	}

	void UndoRemoveBook(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		ChangeBookRemoved(model, index, indexList, std::move(item), std::move(callback), false);
	}

	void AddToNewGroup(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		GroupAction(model, index, indexList, std::move(item), std::move(callback), &GroupController::AddToGroup);
	}

	void AddToGroup(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		GroupAction(model, index, indexList, std::move(item), std::move(callback), &GroupController::AddToGroup);
	}

	void RemoveFromGroup(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		GroupAction(model, index, indexList, std::move(item), std::move(callback), &GroupController::RemoveFromGroup);
	}

	void RemoveFromAllGroups(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		GroupAction(model, index, indexList, std::move(item), std::move(callback), &GroupController::RemoveFromGroup);
	}

	void SendAsArchive(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		Send(model, index, indexList, std::move(item), std::move(callback), &BooksExtractor::ExtractAsArchives);
	}

	void SendAsIs(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		Send(model, index, indexList, std::move(item), std::move(callback), &BooksExtractor::ExtractAsIs);
	}

private:
	void Send(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback, const BooksExtractor::Extract f) const
	{
		auto dir = m_uiFactory->GetExistingDirectory(SELECT_SEND_TO_FOLDER);
		if (dir.isEmpty())
			return callback(item);

		std::shared_ptr executor = m_logicFactory->GetExecutor();
		BooksExtractor::Books books;
		std::ranges::transform(GetSelected(model, index, indexList, { Role::Folder, Role::FileName, Role::Size }), std::back_inserter(books), [] (auto && item)
		{
			assert(item.size() == 3);
			return BooksExtractor::Book { std::move(item[0]), std::move(item[1]), item[2].toLongLong() };
		});

		auto extractor = m_logicFactory->CreateBooksExtractor();
		((*extractor).*f)(dir, std::move(books), [extractor, item = std::move(item), callback = std::move(callback)] () mutable
		{
			callback(item);
			extractor.reset();
		});
	}

	void GroupAction(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback, const GroupActionFunction & f) const
	{
		const auto id = item->GetData(MenuItem::Column::Parameter).toLongLong();
		((*m_groupController).*f)(id, GetSelected(model, index, indexList), [item = std::move(item), callback = std::move(callback)]
		{
			callback(item);
		});
	}

	GroupController::Ids GetSelected(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList) const
	{
		auto selected = GetSelected(model, index, indexList, { Role::Id });

		GroupController::Ids ids;
		ids.reserve(selected.size());
		std::ranges::transform(selected, std::inserter(ids, ids.end()), [] (const auto & item) { return item.front().toLongLong(); });
		return ids;
	}

	void ChangeBookRemoved(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback, const bool remove) const
	{
		m_databaseUser->Execute({ "Remove books", [&, ids = GetSelected(model, index, indexList), item = std::move(item), callback = std::move(callback), remove] () mutable
		{
			const auto db = m_databaseUser->Database();
			const auto transaction = db->CreateTransaction();
			const auto command = transaction->CreateCommand("insert or replace into Books_User(BookID, IsDeleted) values(:id, :is_deleted)");
			for (const auto & id : ids)
			{
				command->Bind(":id", id);
				command->Bind(":is_deleted", remove ? 1 : 0);
				command->Execute();
			}
			transaction->Commit();

			return [&, item = std::move(item), callback = std::move(callback)] (size_t)
			{
				callback(item);
				m_dataProvider->RequestBooks(true);
			};
		} });
	}

	std::vector<std::vector<QString>> GetSelected(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, const std::vector<int> & roles) const
	{
		QModelIndexList selected;
		model->setData({}, QVariant::fromValue(SelectedRequest { index, indexList, &selected }), Role::Selected);

		std::vector<std::vector<QString>> result;
		result.reserve(selected.size());
		std::ranges::transform(selected, std::back_inserter(result), [&] (const auto & selectedIndex)
		{
			std::vector<QString> resultItem;
			std::ranges::transform(roles, std::back_inserter(resultItem), [&] (const int role) { return selectedIndex.data(role).toString(); });
			return resultItem;
		});

		return result;
	}

private:
	PropagateConstPtr<DatabaseUser, std::shared_ptr> m_databaseUser;
	PropagateConstPtr<ILogicFactory, std::shared_ptr> m_logicFactory;
	PropagateConstPtr<IUiFactory, std::shared_ptr> m_uiFactory;
	PropagateConstPtr<GroupController, std::shared_ptr> m_groupController;
	PropagateConstPtr<DataProvider, std::shared_ptr> m_dataProvider;
};

BooksContextMenuProvider::BooksContextMenuProvider(std::shared_ptr<DatabaseUser> databaseUser
	, std::shared_ptr<ILogicFactory> logicFactory
	, std::shared_ptr<IUiFactory> uiFactory
	, std::shared_ptr<GroupController> groupController
	, std::shared_ptr<DataProvider> dataProvider
)
	: m_impl(std::move(databaseUser)
		, std::move(logicFactory)
		, std::move(uiFactory)
		, std::move(groupController)
		, std::move(dataProvider)
	)
{
	PLOGD << "BooksContextMenuProvider created";
}

BooksContextMenuProvider::~BooksContextMenuProvider()
{
	PLOGD << "BooksContextMenuProvider destroyed";
}

void BooksContextMenuProvider::Request(const QModelIndex & index, Callback callback)
{
	m_impl->Request(index, std::move(callback));
}

void BooksContextMenuProvider::OnContextMenuTriggered(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const
{
	const auto invoker = FindSecond(MENU_HANDLERS, static_cast<MenuAction>(item->GetData(MenuItem::Column::Id).toInt()));
	std::invoke(invoker, *m_impl, model, std::cref(index), std::cref(indexList), std::move(item), std::move(callback));
}
