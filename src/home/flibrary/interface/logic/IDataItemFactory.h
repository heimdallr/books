#pragma once

#include "interface/logic/IDataItem.h"

namespace HomeCompa::Flibrary
{

class IDataItemFactory // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IDataItemFactory() = default;
#define DATA_ITEM(NAME) virtual IDataItem::Ptr Create##NAME(IDataItem* parent = nullptr) const = 0;
	DATA_ITEMS_X_MACRO
#undef DATA_ITEM

	virtual int BookItemRemapColumn(int column) const = 0;
};

}
