#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IProgressController.h"

namespace HomeCompa::Flibrary::Test {

class MockProgressController : public IMainProgressController
{
public: // IProgressController
	MOCK_METHOD(bool, IsStarted, (), (const, noexcept, override));
	MOCK_METHOD(std::unique_ptr<IProgressItem>, Add, (int64_t value), (override));
	MOCK_METHOD(double, GetValue, (), (const, noexcept, override));
	MOCK_METHOD(void, Stop, (), (override));
	MOCK_METHOD(void, RegisterObserver, (IObserver * observer), (override));
	MOCK_METHOD(void, UnregisterObserver, (IObserver * observer), (override));
};

static_assert(!std::is_abstract_v<MockProgressController>, "MockProgressController is abstract!");

}
