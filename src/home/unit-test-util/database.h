#pragma once

#include <concepts>
#include <memory>

#include "export/unit-test-util.h"

namespace HomeCompa::DB {

class IDatabase;
class IDatabaseWritable;

}

namespace HomeCompa::Test {

namespace Details {

template <typename T, typename... AllowedTypes>
concept IsOneOf = (... || std::same_as<T, AllowedTypes>);

template <typename T>
concept ExportedDatabaseType = IsOneOf<T, DB::IDatabase, DB::IDatabaseWritable>;

}

template <Details::ExportedDatabaseType T>
UNIT_TEST_UTIL_EXPORT std::unique_ptr<T> CreateDatabase();

extern template UNIT_TEST_UTIL_EXPORT std::unique_ptr<DB::IDatabase> CreateDatabase();
extern template UNIT_TEST_UTIL_EXPORT std::unique_ptr<DB::IDatabaseWritable> CreateDatabase();

}
