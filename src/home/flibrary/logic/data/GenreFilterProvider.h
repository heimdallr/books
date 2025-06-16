#pragma once

#include <memory>

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IGenreFilterProvider.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class GenreFilterProvider final : virtual public IGenreFilterController
{
	NON_COPY_MOVABLE(GenreFilterProvider)

public:
	GenreFilterProvider(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<ISettings> settings);
	~GenreFilterProvider() override;

private: // IGenreFilterProvider
	[[nodiscard]] std::unordered_set<QString> GetFilteredGenreCodes() const override;
	[[nodiscard]] std::unordered_set<QString> GetFilteredGenreNames() const override;
	[[nodiscard]] const std::unordered_map<QString, QString>& GetGenreNameToCodeMap() const override;
	[[nodiscard]] const std::unordered_map<QString, QString>& GetGenreCodeToNameMap() const override;

private: // IGenreFilterController
	void SetFilteredGenres(const std::unordered_set<QString>& codes) override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
