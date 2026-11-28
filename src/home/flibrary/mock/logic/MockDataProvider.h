#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IDataProvider.h"

namespace HomeCompa::Flibrary::Test {

class MockNavigationInfoProvider
    : public INavigationInfoProvider
{
public: // IDataProviderDetails::IDataProvider
	MOCK_METHOD(void, RequestBooks, (bool force), (const, override));
	MOCK_METHOD(NavigationMode, GetNavigationMode, (), (const, override));
	MOCK_METHOD(const QString&, GetNavigationID, (), (const, noexcept, override));

public: // INavigationInfoProvider
	MOCK_METHOD(void, SetNavigationId, (QString id, bool force), (override));
	MOCK_METHOD(void, SetNavigationMode, (NavigationMode navigationMode), (override));
	MOCK_METHOD(void, SetNavigationRequestCallback, (Callback callback), (override));
	MOCK_METHOD(void, RequestNavigation, (bool force), (const, override));
};

class MockBookInfoProvider
    : public IBookInfoProvider
{
public: // IDataProviderDetails::IDataProvider
	MOCK_METHOD(void, RequestBooks, (bool force), (const, override));
	MOCK_METHOD(NavigationMode, GetNavigationMode, (), (const, override));
	MOCK_METHOD(const QString&, GetNavigationID, (), (const, noexcept, override));

public: // IBookInfoProvider
	MOCK_METHOD(void, SetBookRequestCallback, (Callback callback), (override));
	MOCK_METHOD(void, SetBooksViewMode, (ViewMode viewMode), (override));
	MOCK_METHOD(BookInfo, GetBookInfo, (long long id), (const, override));
	MOCK_METHOD(void, RequestRoot, (), (override));
	MOCK_METHOD(void, RegisterObserver, (IObserver * observer), (override));
	MOCK_METHOD(void, UnregisterObserver, (IObserver * observer), (override));
};

class MockAbstractDataProvider
    : public MockNavigationInfoProvider
    , public MockBookInfoProvider
{
};

class MockDataProvider : public MockAbstractDataProvider
{
};

class MockFilterDataProvider : public MockNavigationInfoProvider
{
};

static_assert(!std::is_abstract_v<MockNavigationInfoProvider>, "MockNavigationInfoProvider is abstract!");
static_assert(!std::is_abstract_v<MockBookInfoProvider>, "MockBookInfoProvider is abstract!");
static_assert(!std::is_abstract_v<MockDataProvider>, "MockDataProvider is abstract!");
static_assert(!std::is_abstract_v<MockFilterDataProvider>, "MockFilterDataProvider is abstract!");

} // namespace HomeCompa::Flibrary::Test
