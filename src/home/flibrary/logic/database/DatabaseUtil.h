#pragma once

#include <unordered_set>

#include "interface/logic/IDataItem.h"

namespace HomeCompa::DB
{
class IDatabase;
class IQuery;
}

namespace HomeCompa::Flibrary
{
class IProgressController;
struct QueryInfo;
}

namespace HomeCompa::Flibrary::DatabaseUtil
{
IDataItem::Ptr CreateSimpleListItem(const DB::IQuery& query, const QueryInfo& queryInfo);
IDataItem::Ptr CreateSeriesItem(const DB::IQuery& query, const QueryInfo& queryInfo);
IDataItem::Ptr CreateGenreItem(const DB::IQuery& query, const QueryInfo& queryInfo);
IDataItem::Ptr CreateLanguageItem(const DB::IQuery& query, const QueryInfo& queryInfo);
IDataItem::Ptr CreateFullAuthorItem(const DB::IQuery& query, const QueryInfo& queryInfo);
IDataItem::Ptr CreateBookItem(const DB::IQuery& query);

bool ChangeBookRemoved(DB::IDatabase& db, const std::unordered_set<long long>& ids, bool remove = true, const std::shared_ptr<IProgressController>& progressController = {});

}
