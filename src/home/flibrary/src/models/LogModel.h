#pragma once

class QAbstractItemModel;
class QObject;

namespace HomeCompa::Flibrary {

QAbstractItemModel * CreateLogModel(QObject * parent = nullptr);

}
