#pragma once

class QString;

#include "constants/UserData.h"

namespace HomeCompa::DB {
class IDatabase;
}

namespace HomeCompa::Util {
class IExecutor;
}

namespace HomeCompa::Flibrary::UserData {

void Restore(Util::IExecutor & executor, DB::IDatabase & db, QString fileName, Callback callback);

}
