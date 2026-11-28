#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IUserDataController.h"

namespace HomeCompa::Flibrary::Test {

class MockUserDataController : public IUserDataController
{
public: // IUserDataController
	MOCK_METHOD(void, Backup, (Callback callback), (const, override));
	MOCK_METHOD(void, Restore, (Callback callback), (const, override));
	MOCK_METHOD(void, Backup, (const QString& path, Callback callback), (const, override));
	MOCK_METHOD(void, Restore, (const QString& path, Callback callback), (const, override));
	MOCK_METHOD(void, Backup, (QIODevice & stream, Callback callback), (const, override));
	MOCK_METHOD(void, Restore, (QIODevice & stream, RestoreMode mode, Callback callback), (const, override));
};

static_assert(!std::is_abstract_v<MockUserDataController>, "MockUserDataController is abstract!");

} // namespace HomeCompa::Flibrary::Test
