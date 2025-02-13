#include "CommandExecutor.h"

#include <QDir>
#include <Windows.h>

using namespace HomeCompa::Flibrary;

namespace {

bool Execute(const std::wstring & file, const std::wstring & parameters)
{
	SHELLEXECUTEINFO lpExecInfo {};
	lpExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	lpExecInfo.lpFile = file.data();
	lpExecInfo.fMask = SEE_MASK_DOENVSUBST | SEE_MASK_NOCLOSEPROCESS;
	lpExecInfo.hwnd = nullptr;
	lpExecInfo.lpVerb = L"open";
	lpExecInfo.lpParameters = parameters.data();
	lpExecInfo.lpDirectory = nullptr;
	lpExecInfo.nShow = SW_HIDE;
	lpExecInfo.hInstApp = reinterpret_cast<HINSTANCE>(SE_ERR_DDEFAIL);
	ShellExecuteEx(&lpExecInfo);

	if (lpExecInfo.hProcess == nullptr)
		return false;

	WaitForSingleObject(lpExecInfo.hProcess, INFINITE);
	CloseHandle(lpExecInfo.hProcess);
	return true;
}

}

bool CommandExecutor::ExecuteSystem(const IScriptController::Command & command) const
{
	assert(command.type == IScriptController::Command::Type::System);
	const auto cmdLine = QString("/D /C %1 %2").arg(command.command, command.args).toStdWString();
	return Execute(L"cmd.exe", cmdLine);
}

bool CommandExecutor::ExecuteLaunchApp(const IScriptController::Command & command) const
{
	assert(command.type == IScriptController::Command::Type::LaunchApp);
	const auto file = QDir::toNativeSeparators(command.command).toStdWString();
	const auto parameters = command.args.toStdWString();
	return Execute(file, parameters);
}
