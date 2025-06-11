#include "CommandExecutor.h"

#include <Windows.h>

#include <QDir>
#include <QEventLoop>
#include <QProcess>
#include <QRegularExpression>

#include "fnd/FindPair.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

namespace
{

constexpr std::pair<IScriptController::Command::Type, std::tuple<int /*show*/, bool /*wait for finished*/>> TYPES[] {
	{		   IScriptController::Command::Type::System,        { SW_HIDE, true } },
	{ IScriptController::Command::Type::LaunchConsoleApp,        { SW_HIDE, true } },
	{     IScriptController::Command::Type::LaunchGuiApp, { SW_SHOWNORMAL, false } },
};
static_assert(std::size(TYPES) == static_cast<size_t>(IScriptController::Command::Type::Last));

bool ShellExecute(const std::wstring& file, const std::wstring& parameters, const IScriptController::Command::Type type)
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

QStringList SplitStringWithQuotes(const QString& str)
{
	QRegularExpression regex("\"([^\"]*)\"|([^\\s,]+)");
	QRegularExpressionMatchIterator it = regex.globalMatch(str);
	QStringList result;
	while (it.hasNext())
	{
		QRegularExpressionMatch match = it.next();
		if (match.captured(1).length() > 0)
			result.append(match.captured(1));
		else if (match.captured(2).length() > 0)
			result.append(match.captured(2));
	}
	return result;
}

bool CreateProcess(const std::wstring& file, const std::wstring& parameters)
{
	QProcess process;
	QEventLoop eventLoop;
	const auto args = SplitStringWithQuotes(QString::fromStdWString(parameters));

	QByteArray fixed;
	QObject::connect(&process, &QProcess::started, [&] { PLOGV << QString("%1 %2 launched").arg(file, args.join(" ")); });
	QObject::connect(&process,
	                 &QProcess::finished,
	                 [&](const int code, const QProcess::ExitStatus)
	                 {
						 eventLoop.exit(code);
					 });
	QObject::connect(&process, &QProcess::readyReadStandardError, [&] { PLOGV << process.readAllStandardError(); });
	QObject::connect(&process, &QProcess::readyReadStandardOutput, [&] { PLOGV << process.readAllStandardOutput(); });

	process.start(QString::fromStdWString(file), args, QIODevice::ReadWrite);
	process.closeWriteChannel();

	return eventLoop.exec() == 0;
}

} // namespace

bool CommandExecutor::ExecuteSystem(const IScriptController::Command& command) const
{
	assert(command.type == IScriptController::Command::Type::System);
	const auto cmdLine = QString("/D /C %1 %2").arg(command.command, command.args).toStdWString();
	return ShellExecute(L"cmd.exe", cmdLine, IScriptController::Command::Type::System);
}

bool CommandExecutor::ExecuteLaunchConsoleApp(const IScriptController::Command& command) const
{
	assert(command.type == IScriptController::Command::Type::LaunchConsoleApp);
	const auto file = QDir::toNativeSeparators(command.command).toStdWString();
	const auto parameters = command.args.toStdWString();
	return CreateProcess(file, parameters);
}

bool CommandExecutor::ExecuteLaunchGuiApp(const IScriptController::Command& command) const
{
	assert(command.type == IScriptController::Command::Type::LaunchGuiApp);
	const auto file = QDir::toNativeSeparators(command.command).toStdWString();
	const auto parameters = command.args.toStdWString();
	return ShellExecute(file, parameters, IScriptController::Command::Type::LaunchGuiApp);
}
