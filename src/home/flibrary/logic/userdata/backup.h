#pragma once

#include "constants/UserData.h"

class QString;

namespace HomeCompa::Util {

class IExecutor;

}

namespace HomeCompa::Flibrary {

class IDatabaseUser;

}

namespace HomeCompa::Flibrary::UserData {

void Backup(const Util::IExecutor& executor, const IDatabaseUser& databaseUser, QString fileName, Callback callback);

}
