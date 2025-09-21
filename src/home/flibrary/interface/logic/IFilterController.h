#pragma once

#include <QString>

#include "database/interface/ICommand.h"
#include "database/interface/IQuery.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"

#include "IDataItem.h"
#include "IFilterProvider.h"

namespace HomeCompa::Flibrary
{

namespace details
{
template <typename StatementType, typename T>
struct Binder
{
	static void Bind(StatementType& s, size_t index, const QString& value) = delete;
};

template <typename StatementType>
struct Binder<StatementType, int>
{
	static void Bind(StatementType& s, size_t index, const QString& value)
	{
		s.Bind(index, value.toInt());
	}
};

template <typename StatementType>
struct Binder<StatementType, std::string>
{
	static void Bind(StatementType& s, size_t index, const QString& value)
	{
		s.Bind(index, value.toStdString());
	}
};

} // namespace details

class IFilterController : public IFilterProvider
{
public:
	using Callback = std::function<void()>;
	using CommandBinder = void (*)(DB::ICommand& command, size_t index, const QString& value);
	using QueueBinder = void (*)(DB::IQuery& command, size_t index, const QString& value);

	struct FilteredNavigation
	{
		NavigationMode navigationMode { NavigationMode::Unknown };
		const char* navigationTitle { nullptr };
		std::string_view table {};
		std::string_view idField {};
		CommandBinder commandBinder { nullptr };
		QueueBinder queueBinder { nullptr };
	};

public:
	static constexpr FilteredNavigation FILTERED_NAVIGATION_DESCRIPTION[] {
		{ NavigationMode::Authors, Loc::Authors, "Authors", "AuthorID", &details::Binder<DB::ICommand, int>::Bind, &details::Binder<DB::IQuery, int>::Bind },
		{ NavigationMode::Series, Loc::Series, "Series", "SeriesID", &details::Binder<DB::ICommand, int>::Bind, &details::Binder<DB::IQuery, int>::Bind },
		{ NavigationMode::Genres, Loc::Genres, "Genres", "GenreCode", &details::Binder<DB::ICommand, std::string>::Bind, &details::Binder<DB::IQuery, std::string>::Bind },
		{ NavigationMode::PublishYear, Loc::PublishYears },
		{ NavigationMode::Keywords, Loc::Keywords, "Keywords", "KeywordID", &details::Binder<DB::ICommand, int>::Bind, &details::Binder<DB::IQuery, int>::Bind },
		{ NavigationMode::Updates, Loc::Updates },
		{ NavigationMode::Archives, Loc::Archives },
		{ NavigationMode::Languages, Loc::Languages, "Languages", "LanguageCode", &details::Binder<DB::ICommand, std::string>::Bind, &details::Binder<DB::IQuery, std::string>::Bind },
		{ NavigationMode::Groups, Loc::Groups },
		{ NavigationMode::Search, Loc::Search },
		{ NavigationMode::Reviews, Loc::Reviews },
		{ NavigationMode::AllBooks, Loc::AllBooks },
	};

public:
	virtual void SetFilterEnabled(bool enabled) = 0;
	virtual void SetNavigationItemFlags(NavigationMode navigationMode, QStringList navigationIds, IDataItem::Flags flags, Callback callback) = 0;
	virtual void ClearNavigationItemFlags(NavigationMode navigationMode, QStringList navigationIds, IDataItem::Flags flags, Callback callback) = 0;
};

} // namespace HomeCompa::Flibrary
