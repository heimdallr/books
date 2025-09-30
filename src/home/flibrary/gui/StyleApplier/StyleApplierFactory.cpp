#include "StyleApplierFactory.h"

#include <QAction>

#include "Hypodermic/Hypodermic.h"

#include "ColorSchemeApplier.h"
#include "DllStyleApplier.h"
#include "PluginStyleApplier.h"
#include "QssStyleApplier.h"
#include "log.h"

using namespace HomeCompa::Flibrary;

struct StyleApplierFactory::Impl
{
	Hypodermic::Container&     container;
	std::shared_ptr<ISettings> settings;

	Impl(Hypodermic::Container& container, std::shared_ptr<ISettings> settings)
		: container { container }
		, settings { std::move(settings) }
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
#define STYLE_APPLIER_TYPE_ITEM(NAME) &Impl::CreateApplierImpl<NAME##Applier>, //-V1003
			STYLE_APPLIER_TYPE_ITEMS_X_MACRO
#undef STYLE_APPLIER_TYPE_ITEM
		};

		assert(static_cast<size_t>(type) < std::size(creators));
		auto applier = std::invoke(creators[static_cast<size_t>(type)], this);
		assert(applier->GetType() == type);
		return applier;
	}
};

StyleApplierFactory::StyleApplierFactory(Hypodermic::Container& container, std::shared_ptr<ISettings> settings)
	: m_impl(container, std::move(settings))
{
	PLOGV << "StyleApplierFactory created";
}

StyleApplierFactory::~StyleApplierFactory()
{
	PLOGV << "StyleApplierFactory destroyed";
}

std::shared_ptr<IStyleApplier> StyleApplierFactory::CreateStyleApplier(const IStyleApplier::Type type) const
{
	return m_impl->CreateApplier(type);
}

std::shared_ptr<IStyleApplier> StyleApplierFactory::CreateThemeApplier() const
{
	const auto currentType = m_impl->settings->Get(IStyleApplier::THEME_TYPE_KEY, IStyleApplier::THEME_KEY_DEFAULT);
	const auto type        = IStyleApplier::TypeFromString(currentType.toStdString().data());
	return CreateStyleApplier(type);
}

void StyleApplierFactory::CheckAction(const std::vector<QAction*>& actions) const
{
	std::unordered_map<IStyleApplier::Type, std::vector<QAction*>> typedActions;
	for (auto* action : actions)
		typedActions[static_cast<IStyleApplier::Type>(action->property(IStyleApplier::ACTION_PROPERTY_THEME_TYPE).toInt())].emplace_back(action);

	for (auto& [type, actionsToCheck] : typedActions)
	{
		std::ranges::for_each(actionsToCheck, [](auto* action) {
			action->setEnabled(true);
		});
		auto [name, data] = CreateStyleApplier(type)->GetChecked();
		const auto it     = std::ranges::find_if(actionsToCheck, [&](const QAction* action) {
            return action->property(IStyleApplier::ACTION_PROPERTY_THEME_NAME).toString() == name && action->property(IStyleApplier::ACTION_PROPERTY_THEME_FILE).toString() == data;
        });
		if (it == actionsToCheck.end())
			continue;

		auto& action = **it;
		action.setChecked(true);
		action.setEnabled(false);
	}
}
