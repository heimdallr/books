#pragma once

#include <vector>

class QAbstractItemModel;
class QObject;

namespace HomeCompa::Flibrary {

struct SimpleModeItem
{
	QString Value;
	QString Title;
};
using SimpleModeItems = std::vector<SimpleModeItem>;

QAbstractItemModel * CreateSimpleModel(SimpleModeItems items, QObject * parent = nullptr);

}
