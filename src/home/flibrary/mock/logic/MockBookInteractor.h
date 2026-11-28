#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IBookInteractor.h"

namespace HomeCompa::Flibrary::Test {

class MockBookInteractor : public IBookInteractor
{
public: // IBookInteractor
	MOCK_METHOD(void, OnLinkActivated, (long long bookId, const QString& parameters), (const, override));
	MOCK_METHOD(void, OnDoubleClicked, (long long bookId, const QString& parameters), (const, override));
	MOCK_METHOD(void, OnRecentBookMenuTriggered, (long long bookId, const QString& parameters), (const, override));
};

static_assert(!std::is_abstract_v<MockBookInteractor>, "MockBookInteractor is abstract!");

}
