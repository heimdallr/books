#pragma once

#include <vector>

#include <QString>

namespace HomeCompa::Flibrary {

class IScriptController  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	struct Role
	{
		enum
		{
			Mode = Qt::UserRole + 1,
			Type,
			Name,
			Number,
			Observer,
			Up,
			Down,
		};
	};

	enum class Mode
	{
		None,
		Updated,
		Removed,
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
		};

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

public:
	virtual ~IScriptController() = default;

	virtual const Scripts & GetScripts() const noexcept = 0;
	virtual bool InsertScripts(int row, int count) = 0;
	virtual bool RemoveScripts(int row, int count) = 0;
	virtual bool SetScriptType(int n, Script::Type type) = 0;
	virtual bool SetScriptName(int n, QString name) = 0;
	virtual bool SetScriptNumber(int n, int number) = 0;

	virtual const Commands & GetCommands() const noexcept = 0;
	virtual bool InsertCommand(const QString & uid, int row, int count) = 0;

	virtual void Save() = 0;
};

class IScriptControllerProvider  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IScriptControllerProvider() = default;
	virtual std::shared_ptr<IScriptController> GetScriptController() = 0;
};

}
