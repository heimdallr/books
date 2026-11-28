#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IFilterController.h"

namespace HomeCompa::Flibrary::Test {

class MockFilterController : public IFilterController
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

public: // IFilterController
	MOCK_METHOD(void, SetFilterEnabled, (bool enabled), (override));
	MOCK_METHOD(void, Apply, (), (override));
	MOCK_METHOD(void, SetFlags, (NavigationMode navigationMode, QString id, IDataItem::Flags flags), (override));
	MOCK_METHOD(void, SetFlagsAccumulationMode, (NavigationMode navigationMode, const QString& mode), (override));
	MOCK_METHOD(void, SetRating, (const std::optional<int>& min, const std::optional<int>& max, bool hideUnrated), (override));
	MOCK_METHOD(void, SetNavigationItemFlags, (NavigationMode navigationMode, QStringList navigationIds, IDataItem::Flags flags, Callback callback), (override));
	MOCK_METHOD(void, ClearNavigationItemFlags, (NavigationMode navigationMode, QStringList navigationIds, IDataItem::Flags flags, Callback callback), (override));
	MOCK_METHOD(void, HideFiltered, (NavigationMode navigationMode, QPointer<QAbstractItemModel> model, std::weak_ptr<ICallback> callback), (override));
};

static_assert(!std::is_abstract_v<MockFilterController>, "MockFilterController is abstract!");

} // namespace HomeCompa::Flibrary::Test
