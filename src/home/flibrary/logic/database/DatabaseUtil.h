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

constexpr auto BOOKS_QUERY = R"({}
select b.BookID, b.Title, coalesce(b.SeqNumber, -1) SeqNumber, b.UpdateDate, b.LibRate, b.Lang, b.Year, f.FolderTitle, b.FileName, b.BookSize, b.UserRate, b.LibID, b.IsDeleted, l.Flags{}
    {}
    join Folders f on f.FolderID = b.FolderID
    join Languages l on l.LanguageCode = b.Lang
	{}
)";

IDataItem::Ptr CreateSimpleListItem(const DB::IQuery& query);
IDataItem::Ptr CreateSeriesItem(const DB::IQuery& query);
IDataItem::Ptr CreateGenreItem(const DB::IQuery& query);
IDataItem::Ptr CreateLanguageItem(const DB::IQuery& query);
IDataItem::Ptr CreateFullAuthorItem(const DB::IQuery& query);
IDataItem::Ptr CreateBookItem(const DB::IQuery& query);

bool ChangeBookRemoved(DB::IDatabase& db, const std::unordered_set<long long>& ids, bool remove = true, const std::shared_ptr<IProgressController>& progressController = {});

}
