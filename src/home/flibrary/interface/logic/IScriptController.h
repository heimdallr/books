#pragma once

#include <vector>

#include <QString>

#include "export/flint.h"

class QLineEdit;

namespace HomeCompa::Flibrary {

class IScriptController  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	struct RoleBase
	{
		enum
		{
			Mode = Qt::UserRole + 1,
			Type,
			Number,
			Observer,
			Up,
			Down,
			Uid,
			Last,
		};
	};

	struct RoleScript : RoleBase
	{
	};

	struct RoleCommand : RoleBase
	{
	};

	enum class Mode
	{
		None,
		Updated,
		Removed,
	};

	enum class Macro
	{
		SourceFile,
		UserDestinationFolder,
		Title,
		FileName,
		FileExt,
		BaseFileName,
		Uid,
		Author,
		Series,
		SeqNumber,
		FileSize,
	};

	struct Base
	{
		QString uid;
		int number;
		Mode mode { Mode::None };
		void SetUpdated() noexcept
		{
			mode = Mode::Updated;
		}
	};

	struct Script : Base
	{
		enum class Type
		{
			ExportToDevice,
		};

		QString name;
		Type type { Type::ExportToDevice };
	};
	using Scripts = std::vector<Script>;

	struct Command : Base
	{
		enum class Type
		{
			LaunchApp,
			System,
		};

		QString scriptUid;
		QString command;
		QString args;
		Type type { Type::LaunchApp };
	};
	using Commands = std::vector<Command>;

public:
	static constexpr auto s_context = "ScriptController";
	static constexpr std::pair<Script::Type, const char *> s_scriptTypes[]
	{
		{ Script::Type::ExportToDevice, QT_TRANSLATE_NOOP("ScriptController", "ExportToDevice") },
	};

	class ICommandExecutor  // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~ICommandExecutor() = default;
		virtual bool ExecuteSystem(const Command & command) const = 0;
		virtual bool ExecuteLaunchApp(const Command & command) const = 0;
	};
	using CommandExecutorFunctor = bool(ICommandExecutor::*)(const Command & command) const;
	struct CommandDescription
	{
		const char * type;
		CommandExecutorFunctor executor = nullptr;
	};
	static constexpr std::pair<Command::Type, CommandDescription> s_commandTypes[]
	{
		{ Command::Type::LaunchApp, {QT_TRANSLATE_NOOP("ScriptController", "LaunchApp"), &ICommandExecutor::ExecuteLaunchApp} },
		{ Command::Type::System, {QT_TRANSLATE_NOOP("ScriptController", "System"), &ICommandExecutor::ExecuteSystem} },
	};

	static constexpr std::pair<Macro, const char *> s_commandMacros[]
	{
		{ Macro::SourceFile, QT_TRANSLATE_NOOP("ScriptController", "%source_file%") },
		{ Macro::UserDestinationFolder, QT_TRANSLATE_NOOP("ScriptController", "%user_destination_folder%") },
		{ Macro::Title, QT_TRANSLATE_NOOP("ScriptController", "%title%") },
		{ Macro::FileExt, QT_TRANSLATE_NOOP("ScriptController", "%file_ext%") },
		{ Macro::FileName, QT_TRANSLATE_NOOP("ScriptController", "%file_name%") },
		{ Macro::BaseFileName, QT_TRANSLATE_NOOP("ScriptController", "%base_file_name%") },
		{ Macro::Uid, QT_TRANSLATE_NOOP("ScriptController", "%uid%") },
		{ Macro::Author, QT_TRANSLATE_NOOP("ScriptController", "%author%") },
		{ Macro::Series, QT_TRANSLATE_NOOP("ScriptController", "%series%") },
		{ Macro::SeqNumber, QT_TRANSLATE_NOOP("ScriptController", "%seq_number%") },
		{ Macro::FileSize, QT_TRANSLATE_NOOP("ScriptController", "%file_size%") },
	};
public:
	FLINT_EXPORT static bool HasMacro(const QString& str, Macro macro);
	FLINT_EXPORT static QString & SetMacro(QString & str, Macro macro, const QString & value);
	FLINT_EXPORT static const char * GetMacro(Macro macro);
	FLINT_EXPORT static void SetMacroActions(QLineEdit * widget);

public:
	virtual ~IScriptController() = default;

	virtual const Scripts & GetScripts() const noexcept = 0;
	virtual bool InsertScripts(int row, int count) = 0;
	virtual bool RemoveScripts(int row, int count) = 0;
	virtual bool SetScriptType(int n, Script::Type value) = 0;
	virtual bool SetScriptName(int n, QString value) = 0;
	virtual bool SetScriptNumber(int n, int value) = 0;

	virtual const Commands & GetCommands() const noexcept = 0;
	virtual Commands GetCommands(const QString & scriptUid) const = 0;
	virtual bool InsertCommand(const QString & uid, int row, int count) = 0;
	virtual bool RemoveCommand(int row, int count) = 0;
	virtual bool SetCommandType(int n, Command::Type value) = 0;
	virtual bool SetCommandCommand(int n, QString value) = 0;
	virtual bool SetCommandArgs(int n, QString value) = 0;
	virtual bool SetCommandNumber(int n, int value) = 0;

	virtual bool Execute(const Command & command) const = 0;

	virtual void Save() = 0;
};

class IScriptControllerProvider  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IScriptControllerProvider() = default;
	virtual std::shared_ptr<IScriptController> GetScriptController() = 0;
};

}
