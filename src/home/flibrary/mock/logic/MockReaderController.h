#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IReaderController.h"

namespace HomeCompa::Flibrary::Test {

class MockReaderController : public IReaderController
{
public: // IReaderController
	MOCK_METHOD(void, Read, (long long id), (const, override));
	MOCK_METHOD(void, ReadRandomBook, (QString lang), (const, override));
};

static_assert(!std::is_abstract_v<MockReaderController>, "MockReaderController is abstract!");

}
