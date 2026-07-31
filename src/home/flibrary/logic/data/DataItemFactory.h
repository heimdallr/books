#pragma once

#include "interface/logic/IDataItemFactory.h"

namespace HomeCompa::Flibrary
{

class DataItemFactory final : virtual public IDataItemFactory
{
private: // IDataItemFactory
#define DATA_ITEM(NAME) IDataItem::Ptr Create##NAME(IDataItem* parent = nullptr) const override;
	DATA_ITEMS_X_MACRO
#undef DATA_ITEM

	int BookItemRemapColumn(int column) const override;
};

}
