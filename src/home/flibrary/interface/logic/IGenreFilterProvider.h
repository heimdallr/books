#pragma once

#include <unordered_map>
#include <unordered_set>

class QString;

namespace HomeCompa::Flibrary
{

class IGenreFilterProvider // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IGenreFilterProvider() = default;

	[[nodiscard]] virtual std::unordered_set<QString> GetFilteredGenreCodes() const = 0;
	[[nodiscard]] virtual std::unordered_set<QString> GetFilteredGenreNames() const = 0;
	[[nodiscard]] virtual const std::unordered_map<QString, QString>& GetGenreNameToCodeMap() const = 0;
	[[nodiscard]] virtual const std::unordered_map<QString, QString>& GetGenreCodeToNameMap() const = 0;
};

class IGenreFilterController : virtual public IGenreFilterProvider
{
public:
	virtual void SetFilteredGenres(const std::unordered_set<QString>& codes) = 0;
};

} // namespace HomeCompa::Flibrary
