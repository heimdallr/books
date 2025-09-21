#include "FilterController.h"

#include <QString>

#include "fnd/algorithm.h"
#include "fnd/observable.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto FILTER_ENABLED_KEY = "ui/View/UniFilter/enabled";
}

struct FilterController::Impl final : Observable<IObserver>
{
	std::shared_ptr<ISettings> settings;
	bool filterEnabled { settings->Get(FILTER_ENABLED_KEY, false) };

	explicit Impl(std::shared_ptr<ISettings> settings)
		: settings { std::move(settings) }
	{
	}
};

FilterController::FilterController(std::shared_ptr<ISettings> settings)
	: m_impl { std::make_unique<Impl>(std::move(settings)) }
{
}

FilterController::~FilterController() = default;

bool FilterController::IsFilterEnabled() const noexcept
{
	return m_impl->filterEnabled;
}

void FilterController::SetFilterEnabled(const bool enabled)
{
	if (!Util::Set(m_impl->filterEnabled, enabled))
		return;

	m_impl->settings->Set(FILTER_ENABLED_KEY, enabled);
	m_impl->Perform(&IObserver::OnFilterEnabledChanged);
}

void FilterController::SetNavigationItemFlags(const NavigationMode /*navigationMode*/, QStringList /*navigationIds*/, const IDataItem::Flags /*flags*/, Callback callback)
{
	callback();
}

void FilterController::ClearNavigationItemFlags(const NavigationMode /*navigationMode*/, QStringList /*navigationIds*/, const IDataItem::Flags /*flags*/, Callback callback)
{
	callback();
}

void FilterController::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void FilterController::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
