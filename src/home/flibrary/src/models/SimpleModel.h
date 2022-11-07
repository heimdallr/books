#pragma once

#include <vector>

class QAbstractItemModel;
class QObject;

namespace HomeCompa::Flibrary {

#define SIMPLE_MODEL_ITEMS_XMACRO \
		SIMPLE_MODEL_ITEM(Value)  \
		SIMPLE_MODEL_ITEM(Title)  \

struct SimpleModeItem
{
#define SIMPLE_MODEL_ITEM(NAME) QString NAME;
		SIMPLE_MODEL_ITEMS_XMACRO
#undef	SIMPLE_MODEL_ITEM
};
using SimpleModeItems = std::vector<SimpleModeItem>;

QAbstractItemModel * CreateSimpleModel(SimpleModeItems items, QObject * parent = nullptr);

}
