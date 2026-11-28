#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IDatabaseProvider.h"

namespace HomeCompa::Flibrary::Test {

class MockDatabaseController : public IDatabaseController
{
public: // IDatabaseProvider
	MOCK_METHOD(size_t, Execute, (Task<DB::IDatabase> && task, int priority), (const, override));
	MOCK_METHOD(size_t, ExecuteWritable, (Task<DB::IDatabaseWritable> && task, int priority), (const, override));
	MOCK_METHOD(bool, IsDatabaseValid, (), (const, noexcept, override));
	MOCK_METHOD(std::shared_ptr<DB::IDatabase>, GetDatabase, (), (const, override));
	MOCK_METHOD(std::unique_ptr<DB::IDatabaseWritable>, GetDatabaseWritable, (), (const, override));
	MOCK_METHOD(QVariant, GetSetting, (Key key, QVariant defaultValue), (const, override));
	MOCK_METHOD(void, SetSetting, (Key key, const QVariant& value), (const, override));

public: // IDatabaseController
	MOCK_METHOD(void, Reset, (), (override));
	MOCK_METHOD(void, EnableApplicationCursorChange, (bool value), (override));
	MOCK_METHOD(void, RegisterObserver, (IObserver * observer), (override));
	MOCK_METHOD(void, UnregisterObserver, (IObserver * observer), (override));
};

static_assert(!std::is_abstract_v<MockDatabaseController>, "MockDatabaseController is abstract!");

} // namespace HomeCompa::Flibrary::Test
