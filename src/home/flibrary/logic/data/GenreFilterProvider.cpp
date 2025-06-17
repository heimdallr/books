#include "GenreFilterProvider.h"

#include <QString>

#include "data/Genre.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto FILTERED_GENRES_KEY = "ui/Books/GenreFilter/genres";
}

struct GenreFilterProvider::Impl
{
	std::shared_ptr<const IDatabaseUser> databaseUser;
	std::shared_ptr<ISettings> settings;

	std::unordered_map<QString, QString> nameToCode;
	std::unordered_map<QString, QString> codeToName;

	void Update()
	{
		const auto collect = [this](const Genre& genre, const auto& f) -> void
		{
			codeToName.try_emplace(genre.code, genre.name);
			if (genre.children.empty())
				nameToCode.try_emplace(genre.name, genre.code);
			for (const auto& child : genre.children)
				f(child, f);
		};
		collect(Genre::Load(*databaseUser->Database()), collect);
	}
};

GenreFilterProvider::GenreFilterProvider(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<ISettings> settings)
	: m_impl { std::make_unique<Impl>(std::move(databaseUser), std::move(settings)) }
{
}

GenreFilterProvider::~GenreFilterProvider() = default;

std::unordered_set<QString> GenreFilterProvider::GetFilteredGenreCodes() const
{
	auto list = m_impl->settings->Get(FILTERED_GENRES_KEY).toStringList();
	std::unordered_set<QString> filtered { std::make_move_iterator(list.begin()), std::make_move_iterator(list.end()) };
	return filtered;
}

std::unordered_set<QString> GenreFilterProvider::GetFilteredGenreNames() const
{
	if (m_impl->codeToName.empty())
		m_impl->Update();

	std::unordered_set<QString> result;
	std::ranges::transform(m_impl->settings->Get(FILTERED_GENRES_KEY).toStringList(),
	                       std::inserter(result, result.end()),
	                       [&codeToName = m_impl->codeToName](const auto& code)
	                       {
							   const auto it = codeToName.find(code);
							   return it != codeToName.end() ? it->second : QString {};
						   });
	return result;
}

const std::unordered_map<QString, QString>& GenreFilterProvider::GetGenreNameToCodeMap() const
{
	if (m_impl->nameToCode.empty())
		m_impl->Update();

	return m_impl->nameToCode;
}

const std::unordered_map<QString, QString>& GenreFilterProvider::GetGenreCodeToNameMap() const
{
	if (m_impl->codeToName.empty())
		m_impl->Update();

	return m_impl->codeToName;
}

void GenreFilterProvider::SetFilteredGenres(const std::unordered_set<QString>& codes)
{
	m_impl->settings->Set(FILTERED_GENRES_KEY, QStringList { codes.cbegin(), codes.cend() });
}
