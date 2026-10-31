#pragma once

#include "constants/UserData.h"

class QString;

namespace HomeCompa::Util {

class IExecutor;

}

namespace HomeCompa::Flibrary {

class IDatabaseProvider;

}

namespace HomeCompa::Flibrary::UserData {

void Backup(const Util::IExecutor& executor, const IDatabaseProvider& databaseProvider, QString fileName, Callback callback);

}
