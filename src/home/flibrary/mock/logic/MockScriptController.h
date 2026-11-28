#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IScriptController.h"

namespace HomeCompa::Flibrary::Test {

class MockScriptController : public IScriptController
{
public: // IScriptController
	MOCK_METHOD(const Scripts&, GetScripts, (), (const, noexcept, override));
	MOCK_METHOD(bool, InsertScripts, (int row, int count), (override));
	MOCK_METHOD(bool, RemoveScripts, (int row, int count), (override));
	MOCK_METHOD(bool, SetScriptType, (int n, Script::Type value), (override));
	MOCK_METHOD(bool, SetScriptName, (int n, QString value), (override));
	MOCK_METHOD(bool, SetScriptNumber, (int n, int value), (override));
	MOCK_METHOD(const Commands&, GetCommands, (), (const, noexcept, override));
	MOCK_METHOD(Commands, GetCommands, (const QString& scriptUid), (const, override));
	MOCK_METHOD(bool, InsertCommand, (const QString& uid, int row, int count), (override));
	MOCK_METHOD(bool, RemoveCommand, (int row, int count), (override));
	MOCK_METHOD(bool, SetCommandType, (int n, Command::Type value), (override));
	MOCK_METHOD(bool, SetCommandCommand, (int n, QString value), (override));
	MOCK_METHOD(bool, SetCommandArgs, (int n, QString value), (override));
	MOCK_METHOD(bool, SetCommandWorkingFolder, (int n, QString value), (override));
	MOCK_METHOD(bool, SetCommandNumber, (int n, int value), (override));
	MOCK_METHOD(bool, Execute, (const Command& command), (const, override));
	MOCK_METHOD(void, Save, (), (override));
};

static_assert(!std::is_abstract_v<MockScriptController>, "MockScriptController is abstract!");

} // namespace HomeCompa::Flibrary::Test
