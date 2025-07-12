#include "MenuItems.h"

#include <QVariant>

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/Enums.h"

#include "data/DataItem.h"
#include "util/localization.h"

namespace HomeCompa::Flibrary
{

namespace
{

constexpr auto CONTEXT = "BookContextMenu";
constexpr auto GROUPS = QT_TRANSLATE_NOOP("BookContextMenu", "&Groups");
constexpr auto GROUPS_ADD_TO = QT_TRANSLATE_NOOP("BookContextMenu", "&Add to");
constexpr auto GROUPS_ADD_TO_NEW = QT_TRANSLATE_NOOP("BookContextMenu", "&New group...");
constexpr auto GROUPS_REMOVE_FROM = QT_TRANSLATE_NOOP("BookContextMenu", "&Remove from");
constexpr auto GROUPS_REMOVE_FROM_ALL = QT_TRANSLATE_NOOP("BookContextMenu", "&All");

TR_DEF

}

IDataItem::Ptr AddMenuItem(const IDataItem::Ptr& dst, QString title, const int id)
{
	auto item = MenuItem::Create();
	item->SetData(std::move(title), MenuItem::Column::Title);
	item->SetData(QString::number(id), MenuItem::Column::Id);
	return dst->AppendChild(std::move(item));
}

void CreateGroupMenu(const IDataItem::Ptr& root, const QString& id, DB::IDatabase& db)
{
	constexpr auto GROUPS_QUERY = R"(
select g.GroupID, g.Title, coalesce(gl.ObjectID, -1), coalesce(gw.BookID, -1) 
from Groups_User g 
left join Groups_List_User gl on gl.GroupID = g.GroupID and gl.ObjectID = :id 
left join Groups_List_User_View gw on gw.GroupID = g.GroupID and gw.BookID = :id
)";

	const auto parent = AddMenuItem(root, Tr(GROUPS));

	const auto add = AddMenuItem(parent, Tr(GROUPS_ADD_TO), GroupsMenuAction::AddToGroup);
	const auto remove = AddMenuItem(parent, Tr(GROUPS_REMOVE_FROM), GroupsMenuAction::RemoveFromGroup);

	const auto createMenuItem = [&](const DB::IQuery& query) -> IDataItem::Ptr
	{
		if (const auto itemExistsInLinkTable = query.Get<long long>(2) >= 0; itemExistsInLinkTable)
			return AddMenuItem(remove, query.Get<const char*>(1), GroupsMenuAction::RemoveFromGroup);

		if (const auto bookAlreadyExistsInLinkView = query.Get<long long>(3) >= 0; bookAlreadyExistsInLinkView)
			return {};

		return AddMenuItem(add, query.Get<const char*>(1), GroupsMenuAction::AddToGroup);
	};

	const auto query = db.CreateQuery(GROUPS_QUERY);
	query->Bind(":id", id.toInt());
	for (query->Execute(); !query->Eof(); query->Next())
		if (const auto menuItem = createMenuItem(*query))
			menuItem->SetData(QString::number(query->Get<long long>(0)), MenuItem::Column::Parameter);

	if (remove->GetChildCount() > 0)
	{
		AddMenuItem(remove);
		AddMenuItem(remove, Tr(GROUPS_REMOVE_FROM_ALL), GroupsMenuAction::RemoveFromAllGroups)->SetData(QString::number(-1), MenuItem::Column::Parameter);
	}
	else
	{
		AddMenuItem(remove);
		remove->SetData(QVariant(false).toString(), MenuItem::Column::Enabled);
	}

	if (add->GetChildCount() > 0)
		AddMenuItem(add);

	AddMenuItem(add, Tr(GROUPS_ADD_TO_NEW), GroupsMenuAction::AddToNewGroup)->SetData(QString::number(-1), MenuItem::Column::Parameter);
}

void ExecuteGroupAction(const GroupController& controller, GroupActionFunction invoker, const GroupController::Id id, GroupController::Ids ids, GroupController::Callback callback)
{
	std::invoke(invoker, controller, id, std::move(ids), std::move(callback));
}

} // namespace HomeCompa::Flibrary
