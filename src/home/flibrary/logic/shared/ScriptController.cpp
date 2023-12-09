#include "ScriptController.h"

#include <ranges>

#include <QUuid>

#include <Hypodermic/Container.h>
#include <plog/Log.h>

#include "fnd/algorithm.h"
#include "fnd/FindPair.h"
#include "interface/logic/IScriptController.h"
#include "util/ISettings.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto SCRIPTS = "Scripts";
constexpr auto SCRIPT_KEY_TEMPLATE = "%1/%2";
constexpr auto SCRIPT_VALUE_KEY_TEMPLATE = "%1/%2/%3";
constexpr auto NAME = "Name";
constexpr auto NUMBER = "Number";
constexpr auto TYPE = "Type";

}

struct ScriptController::Impl
{
	Scripts scripts;
	Commands commands;
	PropagateConstPtr<ISettings, std::shared_ptr> settings;

	explicit Impl(std::shared_ptr<ISettings> settings)
		: settings(std::move(settings))
	{
		const SettingsGroup scriptsGuard(*this->settings, SCRIPTS);
		std::ranges::transform(this->settings->GetGroups(), std::back_inserter(scripts), [&] (const QString & uid)
		{
			const SettingsGroup scriptGuard(*this->settings, uid);
			Script script { {uid, this->settings->Get(NUMBER).toInt()}, this->settings->Get(NAME).toString(), FindFirst(s_scriptTypes, this->settings->Get(TYPE).toString().toStdString().data(), PszComparer{})};
			return script;
		});
	}

	Script & GetScript(const int n)
	{
		assert(n >= 0 && n < static_cast<int>(scripts.size()));
		return scripts[n];
	}
};

ScriptController::ScriptController(std::shared_ptr<ISettings> settings)
	: m_impl(std::move(settings))
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
	scripts.reserve(count);
	std::generate_n(std::back_inserter(scripts), count, [n = it == m_impl->scripts.end() ? 0 : it->number] () mutable
	{
		return Script { { QUuid::createUuid().toString(QUuid::WithoutBraces), ++n, Mode::Updated }, {}, Script::Type::ExportToDevice };
	});
	m_impl->scripts.insert(std::next(m_impl->scripts.begin(), row), std::make_move_iterator(scripts.begin()), std::make_move_iterator(scripts.end()));
	return true;
}

bool ScriptController::RemoveScripts(const int row, const int count)
{
	for (int i = 0; i < count; ++i)
		m_impl->scripts[row + i].mode = Mode::Removed;

	return true;
}

bool ScriptController::SetScriptType(const int n, const Script::Type type)
{
	auto & item = m_impl->GetScript(n);
	return Util::Set(item.type, type, item, &Base::SetUpdated);
}

bool ScriptController::SetScriptName(const int n, QString name)
{
	auto & item = m_impl->GetScript(n);
	return Util::Set(item.name, std::move(name), item, &Base::SetUpdated);
}

bool ScriptController::SetScriptNumber(const int n, const int number)
{
	auto & item = m_impl->GetScript(n);
	return Util::Set(item.number, number, item, &Base::SetUpdated);
}

void ScriptController::Save()
{
	for (auto & script : m_impl->scripts | std::views::filter([](const auto & item){ return item.mode == Mode::Updated; }))
	{
		m_impl->settings->Set(QString(SCRIPT_VALUE_KEY_TEMPLATE).arg(SCRIPTS).arg(script.uid).arg(NAME), script.name);
		m_impl->settings->Set(QString(SCRIPT_VALUE_KEY_TEMPLATE).arg(SCRIPTS).arg(script.uid).arg(NUMBER), script.number);
		m_impl->settings->Set(QString(SCRIPT_VALUE_KEY_TEMPLATE).arg(SCRIPTS).arg(script.uid).arg(TYPE), FindSecond(s_scriptTypes, script.type));
		script.mode = Mode::None;
	}

	for (auto & script : m_impl->scripts | std::views::filter([](const auto & item){ return item.mode == Mode::Removed; }))
		m_impl->settings->Remove(QString(SCRIPT_KEY_TEMPLATE).arg(SCRIPTS).arg(script.uid));
}

const IScriptController::Commands & ScriptController::GetCommands() const noexcept
{
	return m_impl->commands;
}

bool ScriptController::InsertCommand(const QString & uid, const int row, const int count)
{
	auto filtered = m_impl->commands | std::views::filter([&] (const auto & item) { return item.uid == uid; });
	const auto it = std::ranges::max_element(filtered, [] (const auto & lhs, const auto & rhs) { return lhs.number < rhs.number; });
	Commands commands;
	commands.reserve(count);
	std::generate_n(std::back_inserter(commands), count, [&, n = it == std::ranges::end(filtered) ? 0 : it->number]() mutable
	{
		return Command { { uid, ++n, Mode::Updated }, {}, {}, Command::Type::LaunchApp };
	});
	m_impl->commands.insert(std::next(m_impl->commands.begin(), row), std::make_move_iterator(commands.begin()), std::make_move_iterator(commands.end()));
	return true;
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
