#pragma once

#include "interface/logic/IDataItem.h"

#include "ChangeNavigationController/GroupController.h"

namespace HomeCompa::DB
{
class IDatabase;
}

namespace HomeCompa::Flibrary
{

using GroupActionFunction = void (GroupController::*)(GroupController::Id id, GroupController::Ids ids, GroupController::Callback callback) const;

IDataItem::Ptr AddMenuItem(const IDataItem::Ptr& dst, QString title = {}, int id = -1);
void CreateGroupMenu(const IDataItem::Ptr& root, const QString& id, DB::IDatabase& db);
void ExecuteGroupAction(const GroupController& controller, GroupActionFunction invoker, GroupController::Id id, GroupController::Ids ids, GroupController::Callback callback);

}
