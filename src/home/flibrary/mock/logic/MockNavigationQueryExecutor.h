#pragma once

#include <gmock/gmock.h>

#include "interface/logic/INavigationQueryExecutor.h"

namespace HomeCompa::Flibrary::Test {

class MockNavigationQueryExecutor : public INavigationQueryExecutor
{
public: // INavigationQueryExecutor
	MOCK_METHOD(void, RequestNavigation, (NavigationMode navigationMode, Callback callback, bool force), (const, override));
	MOCK_METHOD(const QueryDescription&, GetQueryDescription, (NavigationMode navigationMode), (const, override));
};

static_assert(!std::is_abstract_v<MockNavigationQueryExecutor>, "MockNavigationQueryExecutor is abstract!");

}
