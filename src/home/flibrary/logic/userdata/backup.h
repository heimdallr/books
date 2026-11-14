#pragma once

#include "constants/UserData.h"

class QIODevice;

namespace HomeCompa::Flibrary {

class IDatabaseProvider;

}

namespace HomeCompa::Flibrary::UserData {

void Backup(const IDatabaseProvider& databaseProvider, QIODevice& stream, Callback callback);

}
