#pragma once

class QString;

namespace HomeCompa::DB {
class Database;
}

namespace HomeCompa::Util {
class Executor;
}

namespace HomeCompa::Flibrary {

void Backup(Util::Executor & executor, DB::Database & db, const QString & fileName);

}
