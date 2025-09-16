#include "GenreFilterProvider.h"

#include <QString>

#include "fnd/observable.h"

#include "data/Genre.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto FILTERED_GENRES_KEY = "ui/Books/GenreFilter/genres";
constexpr auto GENRE_FILTER_ENABLED_KEY = "ui/Books/GenreFilter/enabled";
}

struct GenreFilterProvider::Impl final : Observable<IFilterProvider::IObserver>
{
	std::shared_ptr<const IDatabaseUser> databaseUser;
	std::shared_ptr<ISettings> settings;

	std::unordered_map<QString, QString> nameToCode;
	std::unordered_map<QString, QString> codeToName;

	Impl(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<ISettings> settings)
		: databaseUser { std::move(databaseUser) }
		, settings { std::move(settings) }
	{
	}

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

std::unordered_set<QString> GenreFilterProvider::GetFilteredCodes() const
{
	auto list = m_impl->settings->Get(FILTERED_GENRES_KEY).toStringList();
	std::unordered_set<QString> filtered { std::make_move_iterator(list.begin()), std::make_move_iterator(list.end()) };
	return filtered;
}

std::unordered_set<QString> GenreFilterProvider::GetFilteredNames() const
{
	if (!m_impl->settings->Get(GENRE_FILTER_ENABLED_KEY, false))
		return {};

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

const std::unordered_map<QString, QString>& GenreFilterProvider::GetNameToCodeMap() const
{
	if (m_impl->nameToCode.empty())
		m_impl->Update();

	return m_impl->nameToCode;
}

const std::unordered_map<QString, QString>& GenreFilterProvider::GetCodeToNameMap() const
{
	if (m_impl->codeToName.empty())
		m_impl->Update();

	return m_impl->codeToName;
}

void GenreFilterProvider::SetFilteredCodes(const bool enabled, const std::unordered_set<QString>& codes)
{
	m_impl->settings->Set(GENRE_FILTER_ENABLED_KEY, enabled);
	m_impl->settings->Set(FILTERED_GENRES_KEY, QStringList { codes.cbegin(), codes.cend() });
	m_impl->Perform(&IObserver::OnFilterChanged);
}

void GenreFilterProvider::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void GenreFilterProvider::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
