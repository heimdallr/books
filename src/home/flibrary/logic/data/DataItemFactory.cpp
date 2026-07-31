#include "DataItemFactory.h"

#include "DataItem.h"

using namespace HomeCompa::Flibrary;

namespace 
{

template<typename T>
IDataItem::Ptr CreateImpl(IDataItem* parent)
{
	return T::Create(parent);
}

template <>
IDataItem::Ptr CreateImpl<DataItem>(IDataItem*)
{
	assert(false && "unexpected call");
	return {};
}

}

#define DATA_ITEM(NAME) IDataItem::Ptr DataItemFactory::Create##NAME(IDataItem* parent) const { return CreateImpl<NAME>(parent); }
DATA_ITEMS_X_MACRO
#undef DATA_ITEM

int DataItemFactory::BookItemRemapColumn(const int column) const
{
	return BookItem::Remap(column);
}
