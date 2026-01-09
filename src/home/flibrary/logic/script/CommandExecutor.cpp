#include "CommandExecutor.h"

#include <Windows.h>

#include <QBuffer>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>

#include "fnd/FindPair.h"

#include "network/network/downloader.h"
#include "util/files.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
QStringList SplitStringWithQuotes(const QString& str)
{
	QRegularExpression              regex("\"([^\"]*)\"|([^\\s,]+)");
	QRegularExpressionMatchIterator it = regex.globalMatch(str);
	QStringList                     result;
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

bool EmbeddedCommandDownload(const QString& argStr)
{
	bool    outputFileMode = false;
	QString outputFileName;
	QString url;
	for (const auto& arg : SplitStringWithQuotes(argStr))
	{
		if (arg == "-o")
		{
			outputFileMode = true;
			continue;
		}

		(outputFileMode ? outputFileName : url) = arg;
		outputFileMode                          = false;
	}

	if (outputFileName.isEmpty())
	{
		PLOGE << "Output file name missed";
		return false;
	}
	if (url.isEmpty())
	{
		PLOGE << "Url missed";
		return false;
	}

	QByteArray bytes;
	{
		QBuffer buffer(&bytes);
		buffer.open(QIODevice::WriteOnly);
		QEventLoop          eventLoop;
		Network::Downloader downloader;
		downloader.Download(url, buffer, [&](size_t, const int code, const QString& message) {
			if (!message.isEmpty())
				PLOGE << QString("Download %1 failed: %2. %3").arg(url).arg(code).arg(message);
			eventLoop.exit(code);
		});
		const auto result = eventLoop.exec();
		if (result != 0)
			return false;
	}

	const QFileInfo fileInfo(outputFileName);

	if (const auto outputDir = fileInfo.dir(); !(outputDir.exists() || outputDir.mkpath(".")))
	{
		PLOGE << "Cannot create folder " << outputDir.path();
		return false;
	}

	QFile file(outputFileName);
	if (!file.open(QIODevice::WriteOnly))
	{
		PLOGE << "Cannot write to " << outputFileName;
		return false;
	}

	if (const auto realWritten = file.write(bytes); realWritten != bytes.length())
	{
		PLOGE << "Write to " << outputFileName << "failed: written " << realWritten << " bytes of " << bytes.length();
		return false;
	}

	return true;
}

bool EmbeddedCommandOpenLink(const QString& argStr)
{
	return QDesktopServices::openUrl(argStr);
}

constexpr std::pair<IScriptController::Command::Type, std::tuple<int /*show*/, bool /*wait for finished*/>> TYPES[] {
	{		   IScriptController::Command::Type::System,        { SW_HIDE, true } },
	{ IScriptController::Command::Type::LaunchConsoleApp,        { SW_HIDE, true } },
	{     IScriptController::Command::Type::LaunchGuiApp, { SW_SHOWNORMAL, false } },
	{		 IScriptController::Command::Type::Embedded,  { SW_SHOWNORMAL, true } },
};
static_assert(std::size(TYPES) == static_cast<size_t>(IScriptController::Command::Type::Last));

constexpr std::pair<const char*, bool (*)(const QString&)> EMBEDDED_COMMANDS[] {
#define SCRIPT_CONTROLLER_EMBEDDED_COMMAND_ITEM(NAME) { #NAME, &EmbeddedCommand##NAME },
	SCRIPT_CONTROLLER_EMBEDDED_COMMAND_ITEMS_X_MACRO
#undef SCRIPT_CONTROLLER_EMBEDDED_COMMAND_ITEM
};
static_assert(std::size(EMBEDDED_COMMANDS) == static_cast<size_t>(IScriptController::EmbeddedCommand::Last));

bool ShellExecute(const std::wstring& file, const std::wstring& parameters, const std::wstring& cwd, const IScriptController::Command::Type type)
{
	const auto& [show, wait] = FindSecond(TYPES, type);

	SHELLEXECUTEINFO lpExecInfo {};
	lpExecInfo.cbSize       = sizeof(SHELLEXECUTEINFO);
	lpExecInfo.lpFile       = file.data();
	lpExecInfo.fMask        = SEE_MASK_DOENVSUBST | SEE_MASK_NOCLOSEPROCESS;
	lpExecInfo.hwnd         = nullptr;
	lpExecInfo.lpVerb       = L"open";
	lpExecInfo.lpParameters = parameters.data();
	lpExecInfo.lpDirectory  = cwd.data();
	lpExecInfo.nShow        = show;
	lpExecInfo.hInstApp     = reinterpret_cast<HINSTANCE>(SE_ERR_DDEFAIL);
	ShellExecuteEx(&lpExecInfo);

	if (lpExecInfo.hProcess == nullptr)
		return false;

	if (wait)
		WaitForSingleObject(lpExecInfo.hProcess, INFINITE);

	CloseHandle(lpExecInfo.hProcess);
	return true;
}

bool CreateProcess(const std::wstring& file, const std::wstring& parameters, const std::wstring& cwd)
{
	QProcess   process;
	QEventLoop eventLoop;
	process.setWorkingDirectory(QString::fromStdWString(cwd));
	const auto args = SplitStringWithQuotes(QString::fromStdWString(parameters));

	QByteArray fixed;
	int       errorCode = 0;
	QObject::connect(&process, &QProcess::started, [&] {
		PLOGV << QString("%1 %2 launched").arg(file, args.join(" "));
	});
	QObject::connect(&process, &QProcess::errorOccurred, [&](const auto error) {
		errorCode = static_cast<int>(error) + 1;
		PLOGE << QString("%1 %2 error: %3").arg(file, args.join(" ")).arg(errorCode);
		eventLoop.exit(errorCode);
	});
	QObject::connect(&process, &QProcess::finished, [&](const int code, const QProcess::ExitStatus) {
		eventLoop.exit(code);
	});
	QObject::connect(&process, &QProcess::readyReadStandardError, [&] {
		PLOGE << process.readAllStandardError();
	});
	QObject::connect(&process, &QProcess::readyReadStandardOutput, [&] {
		PLOGV << process.readAllStandardOutput();
	});

	process.start(QString::fromStdWString(file), args, QIODevice::ReadWrite);
	if (errorCode)
		return errorCode;

	process.closeWriteChannel();

	return eventLoop.exec() == 0;
}

} // namespace

bool CommandExecutor::ExecuteSystem(const IScriptController::Command& command) const
{
	assert(command.type == IScriptController::Command::Type::System);
	const auto cmdLine = QString("/D /C %1 %2").arg(command.command, command.args).toStdWString();
	return ShellExecute(L"cmd.exe", cmdLine, command.workingFolder.toStdWString(), IScriptController::Command::Type::System);
}

bool CommandExecutor::ExecuteLaunchConsoleApp(const IScriptController::Command& command) const
{
	assert(command.type == IScriptController::Command::Type::LaunchConsoleApp);
	const auto file       = QDir::toNativeSeparators(Util::ToAbsolutePath(command.command)).toStdWString();
	const auto parameters = command.args.toStdWString();
	const auto cwd = command.workingFolder.toStdWString();
	return CreateProcess(file, parameters, cwd);
}

bool CommandExecutor::ExecuteLaunchGuiApp(const IScriptController::Command& command) const
{
	assert(command.type == IScriptController::Command::Type::LaunchGuiApp);
	const auto file       = QDir::toNativeSeparators(Util::ToAbsolutePath(command.command)).toStdWString();
	const auto parameters = command.args.toStdWString();
	return ShellExecute(file, parameters, command.workingFolder.toStdWString(), IScriptController::Command::Type::LaunchGuiApp);
}

bool CommandExecutor::ExecuteEmbeddedCommand(const IScriptController::Command& command) const
{
	assert(command.type == IScriptController::Command::Type::Embedded);
	const auto invoker = FindSecond(EMBEDDED_COMMANDS, command.command.toStdString().data(), PszComparer {});
	return std::invoke(invoker, command.args);
}
