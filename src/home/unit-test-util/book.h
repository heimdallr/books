#pragma once

#include "database/interface/IDatabase.h"

#include "util/BookUtil.h"

#include "export/unit-test-util.h"

namespace HomeCompa::DB {

class IDatabase;

}

namespace HomeCompa::Test {

UNIT_TEST_UTIL_EXPORT Util::ExtractedBooks GetExtractedBooks(DB::IDatabase& db);

}
