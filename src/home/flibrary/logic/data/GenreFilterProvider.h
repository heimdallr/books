#pragma once

#include <memory>

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IBooksFilterProvider.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class GenreFilterProvider final
	: public IGenreFilterProvider
	, public IGenreFilterController
{
	NON_COPY_MOVABLE(GenreFilterProvider)

public:
	GenreFilterProvider(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<ISettings> settings);
	~GenreFilterProvider() override;

private: // IGenreFilterProvider
	[[nodiscard]] std::unordered_set<QString> GetFilteredCodes() const override;
	[[nodiscard]] std::unordered_set<QString> GetFilteredNames() const override;
	[[nodiscard]] const std::unordered_map<QString, QString>& GetNameToCodeMap() const override;
	[[nodiscard]] const std::unordered_map<QString, QString>& GetCodeToNameMap() const override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private: // IGenreFilterController
	void SetFilteredCodes(bool enabled, const std::unordered_set<QString>& codes) override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
