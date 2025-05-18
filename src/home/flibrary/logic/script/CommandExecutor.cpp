#include "CommandExecutor.h"

#include <Windows.h>

#include <QDir>

#include "fnd/FindPair.h"

using namespace HomeCompa::Flibrary;

namespace
{

constexpr std::pair<IScriptController::Command::Type, std::tuple<int /*show*/, bool /*wait for finished*/>> TYPES[] {
	{		   IScriptController::Command::Type::System,        { SW_HIDE, true } },
	{ IScriptController::Command::Type::LaunchConsoleApp,        { SW_HIDE, true } },
	{     IScriptController::Command::Type::LaunchGuiApp, { SW_SHOWNORMAL, false } },
};
static_assert(std::size(TYPES) == static_cast<size_t>(IScriptController::Command::Type::Last));

bool Execute(const std::wstring& file, const std::wstring& parameters, const IScriptController::Command::Type type)
{
	const auto& [show, wait] = FindSecond(TYPES, type);

	SHELLEXECUTEINFO lpExecInfo {};
	lpExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	lpExecInfo.lpFile = file.data();
	lpExecInfo.fMask = SEE_MASK_DOENVSUBST | SEE_MASK_NOCLOSEPROCESS;
	lpExecInfo.hwnd = nullptr;
	lpExecInfo.lpVerb = L"open";
	lpExecInfo.lpParameters = parameters.data();
	lpExecInfo.lpDirectory = nullptr;
	lpExecInfo.nShow = show;
	lpExecInfo.hInstApp = reinterpret_cast<HINSTANCE>(SE_ERR_DDEFAIL);
	ShellExecuteEx(&lpExecInfo);

	if (lpExecInfo.hProcess == nullptr)
		return false;

	if (wait)
		WaitForSingleObject(lpExecInfo.hProcess, INFINITE);

	CloseHandle(lpExecInfo.hProcess);
	return true;
}

} // namespace

bool CommandExecutor::ExecuteSystem(const IScriptController::Command& command) const
{
	assert(command.type == IScriptController::Command::Type::System);
	const auto cmdLine = QString("/D /C %1 %2").arg(command.command, command.args).toStdWString();
	return Execute(L"cmd.exe", cmdLine, IScriptController::Command::Type::System);
}

bool CommandExecutor::ExecuteLaunchConsoleApp(const IScriptController::Command& command) const
{
	assert(command.type == IScriptController::Command::Type::LaunchConsoleApp);
	const auto file = QDir::toNativeSeparators(command.command).toStdWString();
	const auto parameters = command.args.toStdWString();
	return Execute(file, parameters, IScriptController::Command::Type::LaunchConsoleApp);
}

bool CommandExecutor::ExecuteLaunchGuiApp(const IScriptController::Command& command) const
{
	assert(command.type == IScriptController::Command::Type::LaunchGuiApp);
	const auto file = QDir::toNativeSeparators(command.command).toStdWString();
	const auto parameters = command.args.toStdWString();
	return Execute(file, parameters, IScriptController::Command::Type::LaunchGuiApp);
}
