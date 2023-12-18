#pragma once

class QMenu;
class QAbstractItemView;

namespace HomeCompa::Flibrary {

class IDataItem;
class ITreeViewController;

void FillMenu(QMenu & menu, const IDataItem & item, ITreeViewController & controller, const QAbstractItemView & view);

}
