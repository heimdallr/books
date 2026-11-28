#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IBookSearchController.h"

namespace HomeCompa::Flibrary::Test {

class MockBookSearchController : public IBookSearchController
{
public: // IBookSearchController
	MOCK_METHOD(void, CreateNew, (Callback callback), (override));
	MOCK_METHOD(void, Remove, (Ids ids, Callback callback), (const, override));
	MOCK_METHOD(void, Search, (QString searchString, Callback callback), (override));
};

static_assert(!std::is_abstract_v<MockBookSearchController>, "MockBookSearchController is abstract!");

}
