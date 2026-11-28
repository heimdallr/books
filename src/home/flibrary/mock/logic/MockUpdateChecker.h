#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IUpdateChecker.h"

namespace HomeCompa::Flibrary::Test {

class MockUpdateChecker : public IUpdateChecker
{
public: // IUpdateChecker
	MOCK_METHOD(void, CheckForUpdate, (bool force, Callback callback), (override));
};

static_assert(!std::is_abstract_v<MockUpdateChecker>, "MockUpdateChecker is abstract!");

} // namespace HomeCompa::Flibrary::Test
