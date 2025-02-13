#pragma once

#include "interface/logic/IDataItem.h"

namespace HomeCompa::Flibrary
{
class IBookInfoProvider
{
public:
	virtual ~IBookInfoProvider() = default;
	virtual BookInfo GetBookInfo(long long id) const = 0;
};
} // namespace HomeCompa::Flibrary
