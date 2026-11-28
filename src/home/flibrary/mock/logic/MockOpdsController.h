#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IOpdsController.h"

namespace HomeCompa::Flibrary::Test {

class MockOpdsControllerObserver : public IOpdsController::IObserver
{
public: // IOpdsController::IObserver
	MOCK_METHOD(void, OnRunningChanged, (), (override));
};

class MockOpdsController : public IOpdsController
{
public: // IOpdsController
	MOCK_METHOD(bool, IsRunning, (), (const, override));
	MOCK_METHOD(void, Start, (const QString& cmdLine), (override));
	MOCK_METHOD(void, Stop, (), (override));
	MOCK_METHOD(void, Restart, (), (override));
	MOCK_METHOD(bool, InStartup, (), (const, override));
	MOCK_METHOD(void, AddToStartup, (const QString& cmdLine), (const, override));
	MOCK_METHOD(void, RemoveFromStartup, (), (const, override));
	MOCK_METHOD(void, RegisterObserver, (IObserver * observer), (override));
	MOCK_METHOD(void, UnregisterObserver, (IObserver * observer), (override));
};

static_assert(!std::is_abstract_v<MockOpdsControllerObserver>, "MockOpdsControllerObserver is abstract!");
static_assert(!std::is_abstract_v<MockOpdsController>, "MockOpdsController is abstract!");

} // namespace HomeCompa::Flibrary::Test
