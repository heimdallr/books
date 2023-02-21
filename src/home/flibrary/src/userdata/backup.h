#pragma once

class QString;

namespace HomeCompa::DB {
class IDatabase;
}

namespace HomeCompa::Util {
class IExecutor;
}

namespace HomeCompa::Flibrary {

void Backup(Util::IExecutor & executor, DB::IDatabase & db, QString fileName);

}
