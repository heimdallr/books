#pragma once

#include <QModelIndex>

namespace HomeCompa::Flibrary::ModelUtil
{

void EnumerateLeafs(const QAbstractItemModel& model, const QModelIndexList& indexList, const std::function<void(const QModelIndex&)>& f);

}
