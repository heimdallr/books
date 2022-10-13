#pragma once

#include <QAbstractItemModel>

namespace HomeCompa::DB {
class Database;
}

namespace HomeCompa::Flibrary {

QAbstractItemModel * CreateAuthorsModel(DB::Database & db, QObject * parent = nullptr);

}
