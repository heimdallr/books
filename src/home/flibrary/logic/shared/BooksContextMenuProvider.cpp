#include "BooksContextMenuProvider.h"

#include <ranges>

#include <QModelIndex>
#include <QString>
#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "database/interface/ITransaction.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"
#include "interface/logic/IScriptController.h"
#include "interface/ui/IUiFactory.h"

#include "ChangeNavigationController/GroupController.h"
#include "data/DataItem.h"
#include "data/DataProvider.h"
#include "shared/ReaderController.h"

#include "BooksExtractor.h"
#include "DatabaseUser.h"

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

class IContextMenuHandler  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Function = void (IContextMenuHandler::*)(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, BooksContextMenuProvider::Callback callback) const;

public:
	virtual ~IContextMenuHandler() = default;

#define BOOKS_MENU_ACTION_ITEM(NAME) virtual void NAME(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, BooksContextMenuProvider::Callback callback) const = 0;
		BOOKS_MENU_ACTION_ITEMS_X_MACRO
#undef	BOOKS_MENU_ACTION_ITEM
};

constexpr std::pair<BooksMenuAction, IContextMenuHandler::Function> MENU_HANDLERS[]
{
	{ BooksMenuAction::AddToGroup, &IContextMenuHandler::AddToGroup },
#define BOOKS_MENU_ACTION_ITEM(NAME) { BooksMenuAction::NAME, &IContextMenuHandler::NAME },
		BOOKS_MENU_ACTION_ITEMS_X_MACRO
#undef	BOOKS_MENU_ACTION_ITEM
};

IDataItem::Ptr & Add(const IDataItem::Ptr & dst, QString title = {}, const BooksMenuAction id = BooksMenuAction::None)
{
	auto item = MenuItem::Create();
	item->SetData(std::move(title), MenuItem::Column::Title);
	item->SetData(QString::number(static_cast<int>(id)), MenuItem::Column::Id);
	return dst->AppendChild(std::move(item));
}

void CreateGroupMenu(const IDataItem::Ptr & parent, const QString & id, DB::IDatabase & db)
{
	const auto add = Add(parent, Tr(GROUPS_ADD_TO), BooksMenuAction::AddToGroup);
	const auto remove = Add(parent, Tr(GROUPS_REMOVE_FROM), BooksMenuAction::RemoveFromGroup);

	const auto query = db.CreateQuery(GROUPS_QUERY);
	query->Bind(0, id.toInt());
	for (query->Execute(); !query->Eof(); query->Next())
	{
		const auto needAdd = query->Get<long long>(2) < 0;
		const auto & item = needAdd ? add : remove;
		Add(item, query->Get<const char *>(1), needAdd ? BooksMenuAction::AddToGroup : BooksMenuAction::RemoveFromGroup)
			->SetData(QString::number(query->Get<long long>(0)), MenuItem::Column::Parameter);
	}

	if (remove->GetChildCount() > 0)
	{
		Add(remove);
		Add(remove, Tr(GROUPS_REMOVE_FROM_ALL), BooksMenuAction::RemoveFromAllGroups)
			->SetData(QString::number(-1), MenuItem::Column::Parameter);
	}
	else
	{
		Add(remove);
		remove->SetData(QVariant(false).toString(), MenuItem::Column::Enabled);
	}

	if (add->GetChildCount() > 0)
		Add(add);

	Add(add, Tr(GROUPS_ADD_TO_NEW), BooksMenuAction::AddToNewGroup)
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
		, std::shared_ptr<IScriptController> scriptController
	)
		: m_databaseUser(std::move(databaseUser))
		, m_logicFactory(std::move(logicFactory))
		, m_uiFactory(std::move(uiFactory))
		, m_groupController(std::move(groupController))
		, m_dataProvider(std::move(dataProvider))
		, m_scriptController(std::move(scriptController))
	{
	}

public:
	void Request(const QModelIndex & index, Callback callback)
	{
		auto scripts = m_scriptController->GetScripts();
		std::ranges::sort(scripts, [] (const auto & lhs, const auto & rhs) { return lhs.number < rhs.number; });
		m_databaseUser->Execute({ "Create context menu", [
			  id = index.data(Role::Id).toString()
			, type = index.data(Role::Type).value<ItemType>()
			, removed = index.data(Role::IsRemoved).toBool()
			, callback = std::move(callback)
			, db = m_databaseUser->Database()
			, scripts = std::move(scripts)
		]() mutable
		{
			auto result = MenuItem::Create();

			if (type == ItemType::Books)
				Add(result, Tr(READ_BOOK), BooksMenuAction::ReadBook);

			{
				const auto & send = Add(result, Tr(SEND_TO));
				Add(send, Tr(SEND_AS_ARCHIVE), BooksMenuAction::SendAsArchive);
				Add(send, Tr(SEND_AS_IS), BooksMenuAction::SendAsIs);

				for (const auto & script : scripts)
				{
					const auto & scriptItem = Add(send, script.name, BooksMenuAction::SendAsScript);
					scriptItem->SetData(script.uid, MenuItem::Column::Parameter);
				}
			}

			if (type == ItemType::Books)
				CreateGroupMenu(Add(result, Tr(GROUPS), BooksMenuAction::None), id, *db);

			if (type == ItemType::Books)
				Add(result, Tr(removed ? REMOVE_BOOK_UNDO : REMOVE_BOOK), removed ? BooksMenuAction::UndoRemoveBook : BooksMenuAction::RemoveBook);

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
		Send(model, index, indexList, std::move(item), std::move(callback), &BooksExtractor::ExtractAsArchives, true);
	}

	void SendAsIs(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback) const override
	{
		Send(model, index, indexList, std::move(item), std::move(callback), &BooksExtractor::ExtractAsIs, true);
	}

	void SendAsScript(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item, Callback callback) const override
	{
		const auto commands = m_scriptController->GetCommands(item->GetData(MenuItem::Column::Parameter));
		const auto hasUserDestinationFolder = std::ranges::any_of(commands, [] (const auto & item) { return item.HasMacro(IScriptController::Command::Macro::UserDestinationFolder); });
		Send(model, index, indexList, std::move(item), std::move(callback), &BooksExtractor::ExtractAsScript, hasUserDestinationFolder);
	}

private:
	void Send(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback, const BooksExtractor::Extract f, const bool dstFolderRequired) const
	{
		const auto dir = dstFolderRequired ? m_uiFactory->GetExistingDirectory(SELECT_SEND_TO_FOLDER) : QString();
		if (dstFolderRequired && dir.isEmpty())
			return callback(item);

		BooksExtractor::Books books;
		const std::vector<int> roles { Role::Folder, Role::FileName, Role::Size, Role::AuthorFull, Role::Series, Role::SeqNumber, Role::Title };
		const auto selected = GetSelected(model, index, indexList, roles);
		std::ranges::transform(selected, std::back_inserter(books), [&] (auto && book)
		{
			assert(book.size() == roles.size());
			return BooksExtractor::Book { std::move(book[0]), std::move(book[1]), book[2].toLongLong(), std::move(book[3]), std::move(book[4]), book[5].toInt(), std::move(book[6])};
		});

		auto extractor = m_logicFactory->CreateBooksExtractor();
		const auto parameter = item->GetData(MenuItem::Column::Parameter);
		((*extractor).*f)(dir, parameter, std::move(books), [extractor, item = std::move(item), callback = std::move(callback)] (const bool hasError) mutable
		{
			item->SetData(QString::number(hasError), MenuItem::Column::HasError);
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
	std::shared_ptr<IScriptController> m_scriptController;
};

BooksContextMenuProvider::BooksContextMenuProvider(std::shared_ptr<DatabaseUser> databaseUser
	, std::shared_ptr<ILogicFactory> logicFactory
	, std::shared_ptr<IUiFactory> uiFactory
	, std::shared_ptr<GroupController> groupController
	, std::shared_ptr<DataProvider> dataProvider
	, std::shared_ptr<IScriptController> scriptController
)
	: m_impl(std::move(databaseUser)
		, std::move(logicFactory)
		, std::move(uiFactory)
		, std::move(groupController)
		, std::move(dataProvider)
		, std::move(scriptController)
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
	const auto invoker = FindSecond(MENU_HANDLERS, static_cast<BooksMenuAction>(item->GetData(MenuItem::Column::Id).toInt()));
	std::invoke(invoker, *m_impl, model, std::cref(index), std::cref(indexList), std::move(item), std::move(callback));
}
