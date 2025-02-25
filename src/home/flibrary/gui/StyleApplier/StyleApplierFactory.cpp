#include "StyleApplierFactory.h"

#include <QAction>

#include <Hypodermic/Hypodermic.h>

#include "ColorSchemeApplier.h"
#include "PluginStyleApplier.h"

using namespace HomeCompa::Flibrary;

struct StyleApplierFactory::Impl
{
	Hypodermic::Container& container;

	explicit Impl(Hypodermic::Container& container)
		: container(container)
	{
	}

	template <typename T>
	std::shared_ptr<IStyleApplier> CreateApplierImpl() const
	{
		return container.resolve<T>();
	}

	std::shared_ptr<IStyleApplier> CreateApplier(const IStyleApplier::Type type) const
	{
		using ApplierCreator = std::shared_ptr<IStyleApplier> (Impl::*)() const;
		static constexpr ApplierCreator creators[] {
			&Impl::CreateApplierImpl<ColorSchemeApplier>,
			&Impl::CreateApplierImpl<PluginStyleApplier>,
		};

		assert(static_cast<size_t>(type) < std::size(creators));
		auto applier = std::invoke(creators[static_cast<size_t>(type)], this);
		assert(applier->GetType() == type);
		return applier;
	}
};

StyleApplierFactory::StyleApplierFactory(Hypodermic::Container& container)
	: m_impl(container)
{
}

StyleApplierFactory::~StyleApplierFactory() = default;

std::shared_ptr<IStyleApplier> StyleApplierFactory::CreateStyleApplier(const IStyleApplier::Type type) const
{
	return m_impl->CreateApplier(type);
}

void StyleApplierFactory::CheckAction(const std::vector<QAction*>& actions) const
{
	std::unordered_map<IStyleApplier::Type, std::vector<QAction*>> typedActions;
	for (auto* action : actions)
		typedActions[static_cast<IStyleApplier::Type>(action->property(ACTION_PROPERTY_TYPE).toInt())].emplace_back(action);

	for (auto& [type, actionsToCheck] : typedActions)
	{
		std::ranges::for_each(actionsToCheck, [](auto* action) { action->setEnabled(true); });
		auto [name, data] = CreateStyleApplier(type)->GetChecked();
		const auto it =
			std::ranges::find_if(actionsToCheck, [&](const QAction* action) { return action->property(ACTION_PROPERTY_NAME).toString() == name && action->property(ACTION_PROPERTY_DATA) == data; });
		if (it == actionsToCheck.end())
			continue;

		auto& action = **it;
		action.setChecked(true);
		action.setEnabled(false);
	}
}
