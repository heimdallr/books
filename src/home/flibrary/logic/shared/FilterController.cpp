#include "FilterController.h"

#include <QString>

#include "fnd/ScopedCall.h"
#include "fnd/algorithm.h"
#include "fnd/observable.h"

#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto FILTER_ENABLED_KEY                = "ui/View/UniFilter/enabled";
constexpr auto FILTER_RATING_HIDE_UNRATED_KEY    = "ui/View/UniFilter/Rating/HideUnrated";
constexpr auto FILTER_RATING_MINIMUM_KEY         = "ui/View/UniFilter/Rating/Minimum/Value";
constexpr auto FILTER_RATING_MINIMUM_ENABLED_KEY = "ui/View/UniFilter/Rating/Minimum/Enabled";
constexpr auto FILTER_RATING_MAXIMUM_KEY         = "ui/View/UniFilter/Rating/Maximum/Value";
constexpr auto FILTER_RATING_MAXIMUM_ENABLED_KEY = "ui/View/UniFilter/Rating/Maximum/Enabled";

constexpr auto ADD_FILTER_QUERY   = "update {} set Flags = Flags | ? where {} = ?";
constexpr auto CLEAR_FILTER_QUERY = "update {} set Flags = Flags & ~? where {} = ?";
constexpr auto SET_FILTER_QUERY   = "update {} set Flags = ? where {} = ?";

}

struct FilterController::Impl final : Observable<IObserver>
{
	using Changes = std::array<std::unordered_map<QString, IDataItem::Flags>, static_cast<size_t>(NavigationMode::Last)>;

	std::shared_ptr<const IDatabaseUser> databaseUser;
	std::shared_ptr<ISettings>           settings;
	Changes                              changes;

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
		databaseUser->Execute({ "Apply filter", [this, db = std::move(db), navigationMode, ids = std::move(navigationIds), flags, callback = std::move(callback), queryText]() mutable {
								   const auto& description = GetFilteredNavigationDescription(navigationMode);
								   assert(description.table && description.idField);

								   const auto tr      = db->CreateTransaction();
								   const auto command = tr->CreateCommand(std::vformat(queryText, std::make_format_args(description.table, description.idField)));
								   for (const auto& id : ids)
								   {
									   command->Bind(0, static_cast<int>(flags));
									   description.commandBinder(*command, 1, id);
									   command->Execute();
								   }

								   tr->Commit();
								   return [this, navigationMode, flags, callback = std::move(callback)](size_t) {
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
	return m_impl->settings->Get(FILTER_ENABLED_KEY, false);
}

std::vector<IDataItem::Flags> FilterController::GetFlags(const NavigationMode navigationMode, const std::vector<QString>& ids) const
{
	const auto& description = GetFilteredNavigationDescription(navigationMode);
	if (!description.table || !IsFilterEnabled())
		return std::vector { ids.size(), IDataItem::Flags::None };

	std::vector<IDataItem::Flags> result;

	const auto db    = m_impl->databaseUser->Database();
	const auto query = db->CreateQuery(std::format("select Flags from {} where {} = ?", description.table, description.idField));
	for (const auto& id : ids)
	{
		description.queueBinder(*query, 0, id);
		query->Execute();
		result.emplace_back(query->Eof() ? IDataItem::Flags::None : static_cast<IDataItem::Flags>(query->Get<int>(0)));
		query->Reset();
	}

	return result;
}

bool FilterController::HideUnrated() const noexcept
{
	return m_impl->settings->Get(FILTER_RATING_HIDE_UNRATED_KEY, false);
}

bool FilterController::IsMinimumRateEnabled() const noexcept
{
	return m_impl->settings->Get(FILTER_RATING_MINIMUM_ENABLED_KEY, false);
}

bool FilterController::IsMaximumRateEnabled() const noexcept
{
	return m_impl->settings->Get(FILTER_RATING_MAXIMUM_ENABLED_KEY, false);
}

int FilterController::GetMinimumRate() const noexcept
{
	return m_impl->settings->Get(FILTER_RATING_MINIMUM_KEY, 4);
}

int FilterController::GetMaximumRate() const noexcept
{
	return m_impl->settings->Get(FILTER_RATING_MAXIMUM_KEY, 4);
}

void FilterController::SetFilterEnabled(const bool enabled)
{
	m_impl->settings->Set(FILTER_ENABLED_KEY, enabled);
	m_impl->Perform(&IObserver::OnFilterEnabledChanged);
}

void FilterController::Apply()
{
	auto db = m_impl->databaseUser->Database();
	m_impl->databaseUser->Execute({ "Apply filter", [this, db = std::move(db), changes = std::move(m_impl->changes)] {
									   std::vector<NavigationMode> changed;
									   for (size_t index = 0; auto& change : changes)
									   {
										   const ScopedCall indexGuard([&] {
											   ++index;
										   });
										   if (change.empty())
											   continue;

										   const auto navigationMode = static_cast<NavigationMode>(index);
										   changed.emplace_back(navigationMode);
										   const auto& description = GetFilteredNavigationDescription(navigationMode);
										   assert(description.table && description.idField);

										   const auto tr      = db->CreateTransaction();
										   const auto command = tr->CreateCommand(std::vformat(SET_FILTER_QUERY, std::make_format_args(description.table, description.idField)));
										   for (const auto& [id, flags] : change)
										   {
											   command->Bind(0, static_cast<int>(flags));
											   description.commandBinder(*command, 1, id);
											   command->Execute();
										   }

										   tr->Commit();
									   }
									   return [this, changed = std::move(changed)](size_t) {
										   for (const auto& item : changed)
											   m_impl->Perform(&IObserver::OnFilterNavigationChanged, item);
									   };
								   } });
	m_impl->changes = {};
}

void FilterController::SetFlags(const NavigationMode navigationMode, QString id, const IDataItem::Flags flags)
{
	const auto index = static_cast<size_t>(navigationMode);
	assert(index < m_impl->changes.size());
	m_impl->changes[index][std::move(id)] = flags;
}

void FilterController::SetRating(const std::optional<int>& min, const std::optional<int>& max, const bool hideUnrated)
{
	const auto set = [this](const std::optional<int>& value, const char* valueKey, const char* enabledKey) {
		m_impl->settings->Set(enabledKey, !!value);
		if (value)
			m_impl->settings->Set(valueKey, *value);
	};

	m_impl->settings->Set(FILTER_RATING_HIDE_UNRATED_KEY, hideUnrated);
	set(min, FILTER_RATING_MINIMUM_KEY, FILTER_RATING_MINIMUM_ENABLED_KEY);
	set(max, FILTER_RATING_MAXIMUM_KEY, FILTER_RATING_MAXIMUM_ENABLED_KEY);
}

void FilterController::SetNavigationItemFlags(const NavigationMode navigationMode, QStringList navigationIds, const IDataItem::Flags flags, Callback callback)
{
	m_impl->ProcessNavigationItemFlags(navigationMode, std::move(navigationIds), flags, std::move(callback), ADD_FILTER_QUERY);
}

void FilterController::ClearNavigationItemFlags(const NavigationMode navigationMode, QStringList navigationIds, const IDataItem::Flags flags, Callback callback)
{
	m_impl->ProcessNavigationItemFlags(navigationMode, std::move(navigationIds), flags, std::move(callback), CLEAR_FILTER_QUERY);
}

void FilterController::HideFiltered(NavigationMode navigationMode, QPointer<QAbstractItemModel> model, std::weak_ptr<ICallback> callback)
{
	if (const auto locked = callback.lock())
		locked->OnStarted();

	auto db = m_impl->databaseUser->Database();
	m_impl->databaseUser->Execute({ "Hide filtered", [navigationMode, model = std::move(model), callback = std::move(callback), db = std::move(db)]() mutable {
									   using DescriptionItem = std::tuple<NavigationMode, const char*, const char*, const char*, const char*>;
									   static constexpr DescriptionItem description[] {
										   {   NavigationMode::Authors,       " join Author_List al on al.BookID = b.BookID",         " join Authors a on a.AuthorID = al.AuthorID", "a",    "al.AuthorID" },
										   {    NavigationMode::Series,  " left join Series_List sl on sl.BookID = b.BookID",     " left join Series s on s.SeriesID = sl.SeriesID", "s",    "sl.SeriesID" },
										   {    NavigationMode::Genres,        " join Genre_List gl on gl.BookID = b.BookID",        " join Genres g on g.GenreCode = gl.GenreCode", "g",   "gl.GenreCode" },
										   {  NavigationMode::Keywords, " left join Keyword_List kl on kl.BookID = b.BookID", " left join Keywords k on k.KeywordID = kl.KeywordID", "k",   "kl.KeywordID" },
										   { NavigationMode::Languages,       " join Languages l on l.LanguageCode = b.Lang",                                                    "", "l", "l.LanguageCode" },
									   };

									   std::string flags;
									   std::string from;
									   std::string select;
									   std::ranges::for_each(description, [&](const auto& item) {
										   from.append(std::get<1>(item));
										   if (get<0>(item) == navigationMode)
											   return (void)(select = std::format("{}", std::get<4>(item)));

										   flags.append(std::format(" | coalesce({}.Flags, 0)", std::get<3>(item)));
										   from.append(std::get<2>(item));
									   });
									   const auto query =
										   db->CreateQuery(std::format("select {}, b.BookID, (0 {}) & {} from Books b{}", select, flags, static_cast<int>(IDataItem::Flags::BooksFiltered), from));

									   std::unordered_map<QString, std::pair<std::unordered_set<long long>, std::unordered_set<long long>>> values;
									   auto                                                                                                 now = std::chrono::system_clock::now();
									   for (query->Execute(); !query->Eof(); query->Next())
									   {
										   auto& [nonFiltered, filtered] = values[query->Get<const char*>(0)];
										   const auto bookId             = query->Get<long long>(1);
										   if (query->Get<int>(2) == 0)
										   {
											   if (!filtered.contains(bookId))
												   nonFiltered.emplace(bookId);
										   }
										   else
										   {
											   nonFiltered.erase(bookId);
											   filtered.emplace(bookId);
										   }

										   if (auto time = std::chrono::system_clock::now(); time - now > std::chrono::milliseconds(200))
										   {
											   now = time;
											   if (const auto locked = callback.lock(); !locked || locked->Stopped())
											   {
												   values.clear();
												   break;
											   }
										   }
									   }

									   std::unordered_set<QString> ids;
									   std::ranges::transform(
										   values | std::views::filter([](const auto& item) {
											   return item.second.first.empty();
										   }),
										   std::inserter(ids, ids.end()),
										   [](const auto& item) {
											   return item.first;
										   }
									   );
									   return [model = std::move(model), callback = std::move(callback), ids = std::move(ids)](size_t) mutable {
										   if (model.isNull())
											   return;

										   if (!ids.empty())
											   model->setData({}, QVariant::fromValue(ids), Role::HideFilteredCallback);

										   if (const auto locked = callback.lock())
											   locked->OnFinished();
									   };
								   } });
}

void FilterController::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void FilterController::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
