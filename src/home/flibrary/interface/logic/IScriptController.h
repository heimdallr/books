#pragma once

#include <vector>

#include <QString>

#include "fnd/Lockable.h"

#include "export/flint.h"

class QLineEdit;

namespace HomeCompa::Flibrary
{

class IScriptController // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	struct RoleBase
	{
		enum
		{
			Mode = Qt::UserRole + 1,
			Name,
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

#define SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEMS_X_MACRO           \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(SourceFile)            \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(UserDestinationFolder) \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(Title)                 \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(FileName)              \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(FileExt)               \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(BaseFileName)          \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(Author)                \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(AuthorLastFM)          \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(AuthorLastName)        \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(AuthorFirstName)       \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(AuthorMiddleName)      \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(AuthorF)               \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(AuthorM)               \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(Series)                \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(SeqNumber)             \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(FileSize)              \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(Genre)                 \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(GenreTree)             \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(Keyword)               \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(AllKeywords)           \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(Id)                    \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(LibId)                 \
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(Uid)

	enum class Macro
	{
#define SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(NAME) NAME,
		SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEMS_X_MACRO
#undef SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM
			Last
	};

#define SCRIPT_CONTROLLER_EMBEDDED_COMMAND_ITEMS_X_MACRO SCRIPT_CONTROLLER_EMBEDDED_COMMAND_ITEM(Download)

	enum class EmbeddedCommand
	{
#define SCRIPT_CONTROLLER_EMBEDDED_COMMAND_ITEM(NAME) NAME,
		SCRIPT_CONTROLLER_EMBEDDED_COMMAND_ITEMS_X_MACRO
#undef SCRIPT_CONTROLLER_EMBEDDED_COMMAND_ITEM
			Last
	};

	struct Base
	{
		QString uid;
		int number { -1 };
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
			Undefined,
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
			LaunchConsoleApp,
			LaunchGuiApp,
			System,
			Embedded,
			Last
		};

		QString scriptUid;
		QString command;
		QString args;
		Type type { Type::LaunchConsoleApp };
	};

	using Commands = std::vector<Command>;

public:
	static constexpr auto s_context = "ScriptController";
	static constexpr std::pair<Script::Type, const char*> s_scriptTypes[] {
		{ Script::Type::ExportToDevice, QT_TRANSLATE_NOOP("ScriptController", "ExportToDevice") },
	};

	class ICommandExecutor : public Lockable<ICommandExecutor> // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~ICommandExecutor() = default;
		virtual bool ExecuteSystem(const Command& command) const = 0;
		virtual bool ExecuteLaunchConsoleApp(const Command& command) const = 0;
		virtual bool ExecuteLaunchGuiApp(const Command& command) const = 0;
		virtual bool ExecuteEmbeddedCommand(const Command& command) const = 0;
	};

	using CommandExecutorFunctor = bool (ICommandExecutor::*)(const Command& command) const;

	struct CommandDescription
	{
		const char* type { nullptr };
		CommandExecutorFunctor executor { nullptr };
	};

	static constexpr std::pair<Command::Type, CommandDescription> s_commandTypes[] {
		{ Command::Type::LaunchConsoleApp, { QT_TRANSLATE_NOOP("ScriptController", "LaunchApp"), &ICommandExecutor::ExecuteLaunchConsoleApp } },
		{     Command::Type::LaunchGuiApp,  { QT_TRANSLATE_NOOP("ScriptController", "LaunchGuiApp"), &ICommandExecutor::ExecuteLaunchGuiApp } },
		{		   Command::Type::System,              { QT_TRANSLATE_NOOP("ScriptController", "System"), &ICommandExecutor::ExecuteSystem } },
		{         Command::Type::Embedded,   { QT_TRANSLATE_NOOP("ScriptController", "Embedded"), &ICommandExecutor::ExecuteEmbeddedCommand } },
	};
	static_assert(std::size(s_commandTypes) == static_cast<size_t>(Command::Type::Last));

	static constexpr std::pair<Macro, const char*> s_commandMacros[] {
		{			Macro::SourceFile, QT_TRANSLATE_NOOP("ScriptController",             "%source_file%") },
		{ Macro::UserDestinationFolder, QT_TRANSLATE_NOOP("ScriptController", "%user_destination_folder%") },
		{				 Macro::Title, QT_TRANSLATE_NOOP("ScriptController",                   "%title%") },
		{			  Macro::FileName, QT_TRANSLATE_NOOP("ScriptController",               "%file_name%") },
		{			   Macro::FileExt, QT_TRANSLATE_NOOP("ScriptController",                "%file_ext%") },
		{		  Macro::BaseFileName, QT_TRANSLATE_NOOP("ScriptController",          "%base_file_name%") },
		{				Macro::Author, QT_TRANSLATE_NOOP("ScriptController",                  "%author%") },
		{		  Macro::AuthorLastFM, QT_TRANSLATE_NOOP("ScriptController",          "%author_last_fm%") },
		{        Macro::AuthorLastName, QT_TRANSLATE_NOOP("ScriptController",        "%author_last_name%") },
		{       Macro::AuthorFirstName, QT_TRANSLATE_NOOP("ScriptController",       "%author_first_name%") },
		{      Macro::AuthorMiddleName, QT_TRANSLATE_NOOP("ScriptController",      "%author_middle_name%") },
		{			   Macro::AuthorF, QT_TRANSLATE_NOOP("ScriptController",                "%author_f%") },
		{			   Macro::AuthorM, QT_TRANSLATE_NOOP("ScriptController",                "%author_m%") },
		{				Macro::Series, QT_TRANSLATE_NOOP("ScriptController",                  "%series%") },
		{			 Macro::SeqNumber, QT_TRANSLATE_NOOP("ScriptController",              "%seq_number%") },
		{			  Macro::FileSize, QT_TRANSLATE_NOOP("ScriptController",               "%file_size%") },
		{				 Macro::Genre, QT_TRANSLATE_NOOP("ScriptController",                   "%genre%") },
		{			 Macro::GenreTree, QT_TRANSLATE_NOOP("ScriptController",              "%genre_tree%") },
		{			   Macro::Keyword, QT_TRANSLATE_NOOP("ScriptController",                 "%keyword%") },
		{		   Macro::AllKeywords, QT_TRANSLATE_NOOP("ScriptController",            "%all_keywords%") },
		{					Macro::Id, QT_TRANSLATE_NOOP("ScriptController",                   "%db_id%") },
		{				 Macro::LibId, QT_TRANSLATE_NOOP("ScriptController",                  "%lib_id%") },
		{				   Macro::Uid, QT_TRANSLATE_NOOP("ScriptController",                     "%uid%") },
	};
	static_assert(std::size(s_commandMacros) == static_cast<size_t>(Macro::Last));

public:
	FLINT_EXPORT static bool HasMacro(const QString& str, Macro macro);
	FLINT_EXPORT static QString& SetMacro(QString& str, Macro macro, const QString& value);
	FLINT_EXPORT static const char* GetMacro(Macro macro);
	FLINT_EXPORT static void ExecuteContextMenu(QLineEdit* lineEdit);
	FLINT_EXPORT static QString GetDefaultOutputFileNameTemplate();

public:
	virtual ~IScriptController() = default;

	virtual const Scripts& GetScripts() const noexcept = 0;
	virtual bool InsertScripts(int row, int count) = 0;
	virtual bool RemoveScripts(int row, int count) = 0;
	virtual bool SetScriptType(int n, Script::Type value) = 0;
	virtual bool SetScriptName(int n, QString value) = 0;
	virtual bool SetScriptNumber(int n, int value) = 0;

	virtual const Commands& GetCommands() const noexcept = 0;
	virtual Commands GetCommands(const QString& scriptUid) const = 0;
	virtual bool InsertCommand(const QString& uid, int row, int count) = 0;
	virtual bool RemoveCommand(int row, int count) = 0;
	virtual bool SetCommandType(int n, Command::Type value) = 0;
	virtual bool SetCommandCommand(int n, QString value) = 0;
	virtual bool SetCommandArgs(int n, QString value) = 0;
	virtual bool SetCommandNumber(int n, int value) = 0;

	virtual bool Execute(const Command& command) const = 0;

	virtual void Save() = 0;
};

class IScriptControllerProvider // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IScriptControllerProvider() = default;
	virtual std::shared_ptr<IScriptController> GetScriptController() = 0;
};

} // namespace HomeCompa::Flibrary
