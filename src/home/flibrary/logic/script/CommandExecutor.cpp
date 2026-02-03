#include "CommandExecutor.h"

#include <QBuffer>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QProcess>
#include <QUrl>

#include "fnd/FindPair.h"

#include "network/network/downloader.h"
#include "util/ProcessWrapper.h"
#include "util/files.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

bool EmbeddedCommandDownload(const QString& argStr)
{
	bool    outputFileMode = false;
	QString outputFileName;
	QString url;
	for (const auto& arg : Util::SplitStringWithQuotes(argStr))
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

constexpr std::pair<IScriptController::Command::Type, std::tuple<bool /*show*/, bool /*wait for finished*/>> TYPES[] {
	{		   IScriptController::Command::Type::System, { false, true } },
	{ IScriptController::Command::Type::LaunchConsoleApp, { false, true } },
	{     IScriptController::Command::Type::LaunchGuiApp, { true, false } },
	{		 IScriptController::Command::Type::Embedded,  { true, true } },
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
	return Util::ShellExecuteImpl(file, parameters, cwd, show, wait);
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
	return Util::RunProcess(Util::ToAbsolutePath(command.command), command.args, command.workingFolder);
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
