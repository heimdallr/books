#include "ScriptController.h"

#include <ranges>

#include <QUuid>

#include <Hypodermic/Container.h>
#include <plog/Log.h>

#include "fnd/algorithm.h"
#include "fnd/FindPair.h"
#include "util/ISettings.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto SCRIPTS = "Scripts";
constexpr auto SCRIPT_KEY_TEMPLATE = "%1/%2";
constexpr auto SCRIPT_VALUE_KEY_TEMPLATE = "Scripts/%1/%2";
constexpr auto COMMAND_VALUE_KEY_TEMPLATE = "Scripts/%1/%2/%3";
constexpr auto NAME = "Name";
constexpr auto NUMBER = "Number";
constexpr auto TYPE = "Type";
constexpr auto COMMAND = "Command";
constexpr auto ARGUMENTS = "Arguments";

struct CommandDescriptionComparer
{
	bool operator()(const IScriptController::CommandDescription & lhs, const IScriptController::CommandDescription & rhs) const
	{
		return comparer(lhs.type, rhs.type);
	}

private:
	PszComparer comparer;
};

template <typename T>
auto GetModeFilter(const IScriptController::Mode mode)
{
	return std::views::filter([=] (const T & item)
	{
		return item.mode == mode;
	});
}

auto GetScriptUidFilter(const QString & uid)
{
	return std::views::filter([&] (const IScriptController::Command & item)
	{
		return item.scriptUid == uid;
	});
}

}

struct ScriptController::Impl
{
	Scripts scripts;
	Commands commands;
	PropagateConstPtr<ISettings, std::shared_ptr> settings;
	std::weak_ptr<const ICommandExecutor> commandExecutor;

	Impl(std::shared_ptr<ISettings> settings
		, const std::shared_ptr<const ICommandExecutor> & commandExecutor
	)
		: settings(std::move(settings))
		, commandExecutor(commandExecutor)
	{
		Init();
	}

	Script & GetScript(const int n)
	{
		assert(n >= 0 && n < static_cast<int>(scripts.size()));
		return scripts[static_cast<size_t>(n)];
	}

	Command & GetCommand(const int n)
	{
		assert(n >= 0 && n < static_cast<int>(commands.size()));
		return commands[static_cast<size_t>(n)];
	}

private:
	void Init()
	{
		const SettingsGroup scriptsGuard(*settings, SCRIPTS);
		std::ranges::transform(settings->GetGroups(), std::back_inserter(scripts), [&] (const QString & uid)
		{
			const SettingsGroup scriptGuard(*settings, uid);
			Script script { {uid, settings->Get(NUMBER).toInt() }
				, settings->Get(NAME).toString()
				, FindFirst(s_scriptTypes, settings->Get(TYPE).toString().toStdString().data(), Script::Type::Undefined, PszComparer{})
			};
			std::ranges::transform(settings->GetGroups(), std::back_inserter(commands), [this, &uid] (const QString & commandUid)
			{
				const SettingsGroup commandGuard(*settings, commandUid);
				return Command { {commandUid, settings->Get(NUMBER).toInt() }
					, uid, settings->Get(COMMAND).toString()
					, settings->Get(ARGUMENTS).toString()
					, FindFirst(s_commandTypes, CommandDescription{settings->Get(TYPE).toString().toStdString().data()}, CommandDescriptionComparer{})
				};
			});
			return script;
		});

		const auto undefined = [] (const Script & item) { return item.type == Script::Type::Undefined; };
		for (const auto & script : scripts | std::views::filter(undefined))
			settings->Remove(script.uid);
		if (const auto [begin, end] = std::ranges::remove_if(scripts, undefined); begin != end)
			scripts.erase(begin, end);
	}
};

ScriptController::ScriptController(std::shared_ptr<ISettings> settings
	, const std::shared_ptr<const ICommandExecutor> & commandExecutor
)
	: m_impl(std::move(settings)
		, commandExecutor
	)
{
	PLOGD << "ScriptController created";
}

ScriptController::~ScriptController()
{
	PLOGD << "ScriptController destroyed";
}

const IScriptController::Scripts & ScriptController::GetScripts() const noexcept
{
	return m_impl->scripts;
}

bool ScriptController::InsertScripts(const int row, const int count)
{
	const auto it = std::ranges::max_element(m_impl->scripts, [] (const auto & lhs, const auto & rhs) { return lhs.number < rhs.number; });

	Scripts scripts;
	scripts.reserve(static_cast<size_t>(count));
	std::generate_n(std::back_inserter(scripts), count, [n = it == m_impl->scripts.end() ? 0 : it->number] () mutable
	{
		return Script { { QUuid::createUuid().toString(QUuid::WithoutBraces), ++n, Mode::Updated }, {}, Script::Type::ExportToDevice };
	});
	m_impl->scripts.insert(std::next(m_impl->scripts.begin(), row), std::make_move_iterator(scripts.begin()), std::make_move_iterator(scripts.end()));
	return true;
}

bool ScriptController::RemoveScripts(const int row, const int count)
{
	bool result = false;
	for (size_t i = 0; i < static_cast<size_t>(count); ++i)
		result = Util::Set(m_impl->scripts[static_cast<size_t>(row) + i].mode, Mode::Removed, *this) || result;

	return result;
}

bool ScriptController::SetScriptType(const int n, const Script::Type value)
{
	auto & item = m_impl->GetScript(n);
	return Util::Set(item.type, value, item, &Base::SetUpdated);
}

bool ScriptController::SetScriptName(const int n, QString value)
{
	auto & item = m_impl->GetScript(n);
	return Util::Set(item.name, std::move(value), item, &Base::SetUpdated);
}

bool ScriptController::SetScriptNumber(const int n, const int value)
{
	auto & item = m_impl->GetScript(n);
	return Util::Set(item.number, value, item, &Base::SetUpdated);
}

const IScriptController::Commands & ScriptController::GetCommands() const noexcept
{
	return m_impl->commands;
}

IScriptController::Commands ScriptController::GetCommands(const QString & scriptUid) const
{
	Commands commands;
	std::ranges::copy(m_impl->commands | GetScriptUidFilter(scriptUid), std::back_inserter(commands));
	return commands;
}

bool ScriptController::InsertCommand(const QString & uid, const int row, const int count)
{
	auto filtered = m_impl->commands | GetScriptUidFilter(uid);
	const auto it = std::ranges::max_element(filtered, [] (const auto & lhs, const auto & rhs) { return lhs.number < rhs.number; });
	Commands commands;
	commands.reserve(static_cast<size_t>(count));
	std::generate_n(std::back_inserter(commands), count, [&, n = it == std::ranges::end(filtered) ? 0 : it->number] () mutable
	{
		return Command { { QUuid::createUuid().toString(QUuid::WithoutBraces), ++n, Mode::Updated }, uid, {}, {}, Command::Type::LaunchApp };
	});
	m_impl->commands.insert(std::next(m_impl->commands.begin(), row), std::make_move_iterator(commands.begin()), std::make_move_iterator(commands.end()));
	return true;
}

bool ScriptController::RemoveCommand(const int row, const int count)
{
	bool result = false;
	for (size_t i = 0; i < static_cast<size_t>(count); ++i)
		result = Util::Set(m_impl->commands[static_cast<size_t>(row) + i].mode, Mode::Removed, *this) || result;

	return result;
}

bool ScriptController::SetCommandType(const int n, const Command::Type value)
{
	auto & item = m_impl->GetCommand(n);
	return Util::Set(item.type, value, item, &Base::SetUpdated);
}

bool ScriptController::SetCommandCommand(const int n, QString value)
{
	auto & item = m_impl->GetCommand(n);
	return Util::Set(item.command, std::move(value), item, &Base::SetUpdated);
}

bool ScriptController::SetCommandArgs(const int n, QString value)
{
	auto & item = m_impl->GetCommand(n);
	return Util::Set(item.args, std::move(value), item, &Base::SetUpdated);
}

bool ScriptController::SetCommandNumber(const int n, const int value)
{
	auto & item = m_impl->GetCommand(n);
	return Util::Set(item.number, value, item, &Base::SetUpdated);
}

bool ScriptController::Execute(const Command & command) const
{
	const auto & [type, executor] = FindSecond(s_commandTypes, command.type);
	PLOGD << type << ": " << command.command << " " << command.args;
	return std::invoke(executor, ICommandExecutor::Lock(m_impl->commandExecutor), std::cref(command));
}

void ScriptController::Save()
{
	auto & settings = *m_impl->settings;
	for (auto & script : m_impl->scripts | GetModeFilter<Script>(Mode::Updated))
	{
		const auto scriptValueKeyTemplate = QString(SCRIPT_VALUE_KEY_TEMPLATE).arg(script.uid).arg("%1");
		settings.Set(scriptValueKeyTemplate.arg(NAME), script.name);
		settings.Set(scriptValueKeyTemplate.arg(NUMBER), script.number);
		settings.Set(scriptValueKeyTemplate.arg(TYPE), FindSecond(s_scriptTypes, script.type));
		script.mode = Mode::None;
	}

	for (auto & script : m_impl->scripts | GetModeFilter<Script>(Mode::Removed))
		m_impl->settings->Remove(QString(SCRIPT_KEY_TEMPLATE).arg(SCRIPTS).arg(script.uid));

	for (auto & command : m_impl->commands | GetModeFilter<Command>(Mode::Updated))
	{
		const auto commandValueKeyTemplate = QString(COMMAND_VALUE_KEY_TEMPLATE).arg(command.scriptUid).arg(command.uid).arg("%1");
		settings.Set(commandValueKeyTemplate.arg(COMMAND), command.command);
		settings.Set(commandValueKeyTemplate.arg(ARGUMENTS), command.args);
		settings.Set(commandValueKeyTemplate.arg(NUMBER), command.number);
		settings.Set(commandValueKeyTemplate.arg(TYPE), FindSecond(s_commandTypes, command.type).type);
		command.mode = Mode::None;
	}

	for (auto & command : m_impl->commands | GetModeFilter<Command>(Mode::Removed))
		settings.Remove(QString(SCRIPT_VALUE_KEY_TEMPLATE).arg(command.scriptUid).arg(command.uid));
}

ScriptControllerProvider::ScriptControllerProvider(Hypodermic::Container & container)
	: m_container(container)
{
}

std::shared_ptr<IScriptController> ScriptControllerProvider::GetScriptController()
{
	if (auto controller = m_controller.lock())
		return controller;

	auto controller = m_container.resolve<IScriptController>();
	m_controller = controller;
	return controller;
}
