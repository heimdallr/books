#pragma once

#include <vector>

class QAbstractItemModel;
class QObject;

namespace HomeCompa::Flibrary {

#define SIMPLE_MODEL_ITEMS_XMACRO \
		SIMPLE_MODEL_ITEM(Value)  \
		SIMPLE_MODEL_ITEM(Title)  \

struct SimpleModelItem
{
#define SIMPLE_MODEL_ITEM(NAME) QString NAME;
		SIMPLE_MODEL_ITEMS_XMACRO
#undef	SIMPLE_MODEL_ITEM
};
using SimpleModelItems = std::vector<SimpleModelItem>;

QAbstractItemModel * CreateSimpleModel(SimpleModelItems items, QObject * parent = nullptr);

}
