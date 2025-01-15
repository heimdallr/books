#pragma once

#include "interface/logic/IDataItem.h"

namespace HomeCompa::DB {
class IQuery;
}

namespace HomeCompa::Flibrary::DatabaseUtil {

IDataItem::Ptr CreateSimpleListItem(const DB::IQuery & query, const size_t * index);
IDataItem::Ptr CreateGenreItem(const DB::IQuery & query, const size_t * index);
IDataItem::Ptr CreateFullAuthorItem(const DB::IQuery & query, const size_t * index);
IDataItem::Ptr CreateBookItem(const DB::IQuery & query);

}
