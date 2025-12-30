#pragma once

#include <ranges>

#include "fnd/observer.h"

#include "database/interface/ICommand.h"
#include "database/interface/IQuery.h"

#include "interface/constants/Enums.h"
#include "interface/logic/IModelProvider.h"

#include "export/flint.h"

class QString;

namespace HomeCompa::Flibrary
{

enum class NavigationMode;

class IFilterProvider // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		virtual void OnFilterEnabledChanged()                                 = 0;
		virtual void OnFilterNavigationChanged(NavigationMode navigationMode) = 0;
		virtual void OnFilterBooksChanged()                                   = 0;
	};

	using CommandBinder = void (*)(DB::ICommand& command, size_t index, const QString& value);
	using QueueBinder   = void (*)(DB::IQuery& command, size_t index, const QString& value);
	using ModelGetter   = std::shared_ptr<QAbstractItemModel> (IModelProvider::*)(IDataItem::Ptr data) const;

	struct FilteredNavigation
	{
		NavigationMode navigationMode { NavigationMode::Unknown };
		const char*    navigationTitle { nullptr };
		ModelGetter    modelGetter { nullptr };
		const char*    table { nullptr };
		const char*    idField { nullptr };
		CommandBinder  commandBinder { nullptr };
		QueueBinder    queueBinder { nullptr };
	};

public:
	FLINT_EXPORT static const FilteredNavigation& GetFilteredNavigationDescription(NavigationMode navigationMode);

	static constexpr auto GetDescriptions()
	{
		return std::views::iota(size_t { 0 }, static_cast<size_t>(NavigationMode::Last)) | std::views::transform([](const auto index) {
				   return GetFilteredNavigationDescription(static_cast<NavigationMode>(index));
			   })
		     | std::views::filter([](const auto& description) {
				   return !!description.table;
			   });
	}

public:
	virtual ~IFilterProvider() = default;

	[[nodiscard]] virtual bool                          IsFilterEnabled() const noexcept                                               = 0;
	[[nodiscard]] virtual std::vector<IDataItem::Flags> GetFlags(NavigationMode navigationMode, const std::vector<QString>& ids) const = 0;
	[[nodiscard]] virtual bool                          HideUnrated() const noexcept                                                   = 0;
	[[nodiscard]] virtual bool                          IsMinimumRateEnabled() const noexcept                                          = 0;
	[[nodiscard]] virtual bool                          IsMaximumRateEnabled() const noexcept                                          = 0;
	[[nodiscard]] virtual int                           GetMinimumRate() const noexcept                                                = 0;
	[[nodiscard]] virtual int                           GetMaximumRate() const noexcept                                                = 0;

	virtual void RegisterObserver(IObserver* observer)   = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

} // namespace HomeCompa::Flibrary
