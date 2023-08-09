#include "BooksContextMenuProvider.h"

#include <QModelIndex>
#include <QString>
#include <plog/Log.h>

#include "DatabaseUser.h"
#include "GroupController.h"
#include "data/DataItem.h"
#include "fnd/FindPair.h"
#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"

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

constexpr auto GROUPS_QUERY = "select g.GroupID, g.Title, coalesce(gl.BookID, -1) from Groups_User g left join Groups_List_User gl on gl.GroupID = g.GroupID and gl.BookID = ?";

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
	virtual ~IContextMenuHandler() = default;

#define MENU_ACTION_ITEM(NAME) virtual void NAME(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, BooksContextMenuProvider::Callback callback) const = 0;
		MENU_ACTION_ITEMS_X_MACRO
#undef	MENU_ACTION_ITEM
};

using ContextMenuHandlerFunction = void (IContextMenuHandler::*)(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, BooksContextMenuProvider::Callback callback) const;

constexpr std::pair<MenuAction, ContextMenuHandlerFunction> MENU_HANDLERS[]
{
	{ MenuAction::AddToGroup, &IContextMenuHandler::AddToGroup },
#define MENU_ACTION_ITEM(NAME) { MenuAction::NAME, &IContextMenuHandler::NAME },
		MENU_ACTION_ITEMS_X_MACRO
#undef	MENU_ACTION_ITEM
};

auto Tr(const char * str)
{
	return Loc::Tr(CONTEXT, str);
}

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
		, std::shared_ptr<GroupController> groupController
	)
		: m_databaseUser(std::move(databaseUser))
		, m_groupController(std::move(groupController))
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
void ReadBook(QAbstractItemModel * /*model*/, const QModelIndex & /*index*/, const QList<QModelIndex> & /*indexList*/, IDataItem::Ptr item, Callback callback) const override
{
	callback(item);
}
void RemoveBook(QAbstractItemModel * /*model*/, const QModelIndex & /*index*/, const QList<QModelIndex> & /*indexList*/, IDataItem::Ptr item, Callback callback) const override
{
	callback(item);
}
void UndoRemoveBook(QAbstractItemModel * /*model*/, const QModelIndex & /*index*/, const QList<QModelIndex> & /*indexList*/, IDataItem::Ptr item, Callback callback) const override
{
	callback(item);
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
void SendAsArchive(QAbstractItemModel * /*model*/, const QModelIndex & /*index*/, const QList<QModelIndex> & /*indexList*/, IDataItem::Ptr item, Callback callback) const override
{
	callback(item);
}
void SendAsIs(QAbstractItemModel * /*model*/, const QModelIndex & /*index*/, const QList<QModelIndex> & /*indexList*/, IDataItem::Ptr item, Callback callback) const override
{
	callback(item);
}

using GroupActionFunction = void (GroupController::*)(GroupController::Id id, GroupController::Ids ids, GroupController::Callback callback) const;

private:
	void GroupAction(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item, Callback callback, GroupActionFunction f) const
	{
		const auto id = item->GetData(MenuItem::Column::Parameter).toLongLong();
		((*m_groupController).*f)(id, GetSelected(model, index, indexList), [item = std::move(item), callback = std::move(callback)] () mutable
		{
			callback(item);
		});
	}

	GroupController::Ids GetSelected(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList) const
	{
		QModelIndexList selected;
		model->setData({}, QVariant::fromValue(SelectedRequest { index, indexList, &selected }), Role::Selected);

		GroupController::Ids ids;
		std::ranges::transform(selected, std::inserter(ids, ids.end()), [] (const auto & selectedIndex)
		{
			return selectedIndex.data(Role::Id).toLongLong();
		});
		return ids;
	}

private:
	PropagateConstPtr<DatabaseUser, std::shared_ptr> m_databaseUser;
	PropagateConstPtr<GroupController, std::shared_ptr> m_groupController;
};

BooksContextMenuProvider::BooksContextMenuProvider(std::shared_ptr<DatabaseUser> databaseUser
	, std::shared_ptr<GroupController> groupController
)
	: m_impl(std::move(databaseUser), std::move(groupController))
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
