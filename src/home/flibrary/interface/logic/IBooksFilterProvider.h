#pragma once

#include <unordered_map>
#include <unordered_set>

#include "fnd/observer.h"

class QString;

namespace HomeCompa::Flibrary
{

class IFilterProvider // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		virtual void OnFilterChanged() = 0;
	};

public:
	virtual ~IFilterProvider() = default;

	[[nodiscard]] virtual bool IsFilterEnabled() const = 0;
	[[nodiscard]] virtual std::unordered_set<QString> GetFilteredCodes() const = 0;

	virtual void RegisterObserver(IObserver* observer) = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

class IFilterController  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IFilterController() = default;
	virtual void SetFilteredCodes(bool enabled, const std::unordered_set<QString>& codes) = 0;
};

class IGenreFilterProvider : public IFilterProvider
{
public:
	[[nodiscard]] virtual std::unordered_set<QString> GetFilteredNames() const = 0;
	[[nodiscard]] virtual const std::unordered_map<QString, QString>& GetNameToCodeMap() const = 0;
	[[nodiscard]] virtual const std::unordered_map<QString, QString>& GetCodeToNameMap() const = 0;
};

class IGenreFilterController : public IFilterController
{
};

class ILanguageFilterProvider : public IFilterProvider
{
};

class ILanguageFilterController : public IFilterController
{
};

} // namespace HomeCompa::Flibrary
