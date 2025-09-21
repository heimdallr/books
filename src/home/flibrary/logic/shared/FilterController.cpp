#include "FilterController.h"

#include <QString>

#include "fnd/algorithm.h"
#include "fnd/observable.h"

#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/Enums.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto FILTER_ENABLED_KEY = "ui/View/UniFilter/enabled";

constexpr auto SET_FILTER_QUERY = "update {} set Flags = Flags | ? where {} = ?";
constexpr auto CLEAR_FILTER_QUERY = "update {} set Flags = Flags & ~? where {} = ?";

static_assert(static_cast<size_t>(NavigationMode::Last) == std::size(IFilterController::FILTERED_NAVIGATION_DESCRIPTION));

}

struct FilterController::Impl final : Observable<IObserver>
{
	std::shared_ptr<const IDatabaseUser> databaseUser;
	std::shared_ptr<ISettings> settings;
	bool filterEnabled { settings->Get(FILTER_ENABLED_KEY, true) };

	Impl(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<ISettings> settings)
		: databaseUser { std::move(databaseUser) }
		, settings { std::move(settings) }
	{
	}

	void Notify(const NavigationMode navigationMode, const IDataItem::Flags flags)
	{
		if (!!(flags & IDataItem::Flags::Filtered))
			Perform(&IObserver::OnFilterNavigationChanged, navigationMode);
		if (!!(flags & IDataItem::Flags::BooksFiltered))
			Perform(&IObserver::OnFilterBooksChanged);
	}

	void ProcessNavigationItemFlags(const NavigationMode navigationMode, QStringList navigationIds, const IDataItem::Flags flags, Callback callback, const std::string_view queryText)
	{
		auto db = databaseUser->Database();
		databaseUser->Execute({ "Apply filter",
		                        [this, db = std::move(db), navigationMode, ids = std::move(navigationIds), flags, callback = std::move(callback), queryText]() mutable
		                        {
									const auto tableIndex = static_cast<size_t>(navigationMode);
									assert(tableIndex < std::size(FILTERED_NAVIGATION_DESCRIPTION));
									const auto& description = FILTERED_NAVIGATION_DESCRIPTION[tableIndex];
									assert(!description.table.empty() && !description.idField.empty());

									const auto tr = db->CreateTransaction();
									const auto command = tr->CreateCommand(std::vformat(queryText, std::make_format_args(description.table, description.idField)));
									for (const auto& id : ids)
									{
										command->Bind(0, static_cast<int>(flags));
										description.commandBinder(*command, 1, id);
										command->Execute();
									}

									tr->Commit();
									return [this, navigationMode, flags, callback = std::move(callback)](size_t)
									{
										Notify(navigationMode, flags);
										callback();
									};
								} });
	}
};

FilterController::FilterController(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<ISettings> settings)
	: m_impl { std::make_unique<Impl>(std::move(databaseUser), std::move(settings)) }
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

void FilterController::SetNavigationItemFlags(const NavigationMode navigationMode, QStringList navigationIds, const IDataItem::Flags flags, Callback callback)
{
	m_impl->ProcessNavigationItemFlags(navigationMode, std::move(navigationIds), flags, std::move(callback), SET_FILTER_QUERY);
}

void FilterController::ClearNavigationItemFlags(const NavigationMode navigationMode, QStringList navigationIds, const IDataItem::Flags flags, Callback callback)
{
	m_impl->ProcessNavigationItemFlags(navigationMode, std::move(navigationIds), flags, std::move(callback), CLEAR_FILTER_QUERY);
}

void FilterController::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void FilterController::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
