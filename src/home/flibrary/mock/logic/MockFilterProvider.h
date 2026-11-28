#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IFilterProvider.h"

namespace HomeCompa::Flibrary::Test {

class MockFilterProvider : public IFilterProvider
{
public: // IFilterProvider
	MOCK_METHOD(bool, IsFilterEnabled, (), (const, noexcept, override));
	MOCK_METHOD(std::vector<IDataItem::Flags>, GetFlags, (NavigationMode navigationMode, const std::vector<QString>& ids), (const, override));
	MOCK_METHOD(QString, GetFlagsAccumulationMode, (NavigationMode navigationMode), (const, override));
	MOCK_METHOD(bool, HideUnrated, (), (const, noexcept, override));
	MOCK_METHOD(bool, IsMinimumRateEnabled, (), (const, noexcept, override));
	MOCK_METHOD(bool, IsMaximumRateEnabled, (), (const, noexcept, override));
	MOCK_METHOD(int, GetMinimumRate, (), (const, noexcept, override));
	MOCK_METHOD(int, GetMaximumRate, (), (const, noexcept, override));
	MOCK_METHOD(void, RegisterObserver, (IObserver * observer), (override));
	MOCK_METHOD(void, UnregisterObserver, (IObserver * observer), (override));
};

static_assert(!std::is_abstract_v<MockFilterProvider>, "MockFilterProvider is abstract!");

} // namespace HomeCompa::Flibrary::Test
