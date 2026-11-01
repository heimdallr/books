#pragma once

#include "constants/UserData.h"

class QString;

namespace HomeCompa::Flibrary {

class IDatabaseProvider;

}

namespace HomeCompa::Flibrary::UserData {

void Backup(const IDatabaseProvider& databaseProvider, QString fileName, Callback callback);

}
