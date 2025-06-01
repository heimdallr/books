#include "Genre.h"

#include <ranges>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/GenresLocalization.h"

#include "util/ISettings.h"
#include "util/localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto GENRES_SORT_MODE_KEY = "ui/GenresSortMode";

void Sort(Genre& genre, const auto& proj)
{
	std::ranges::sort(genre.children, {}, proj);
	for (auto& child : genre.children)
		Sort(child, proj);
}

void SortDesc(Genre& genre, const auto& proj)
{
	std::ranges::sort(genre.children, std::greater {}, proj);
	for (auto& child : genre.children)
		Sort(child, proj);
}

void SortByCode(Genre& genre)
{
	Sort(genre, [](const auto& item) { return item.code; });
}

void SortByChildCount(Genre& genre)
{
	Sort(genre, [](const auto& item) { return item.children.size(); });
}

void SortByName(Genre& genre)
{
	Sort(genre, [](const auto& item) { return item.name; });
}

void SortByCodeDesc(Genre& genre)
{
	SortDesc(genre, [](const auto& item) { return item.code; });
}

void SortByNameDesc(Genre& genre)
{
	SortDesc(genre, [](const auto& item) { return item.name; });
}

void SortByChildCountDesc(Genre& genre)
{
	SortDesc(genre, [](const auto& item) { return item.children.size(); });
}

using Sorter = void (*)(Genre&);
constexpr std::pair<const char*, Sorter> SORTERS[] {
#define ITEM(NAME) { #NAME, &(NAME) }
	ITEM(SortByCode), ITEM(SortByName), ITEM(SortByChildCount), ITEM(SortByCodeDesc), ITEM(SortByNameDesc), ITEM(SortByChildCountDesc),
#undef ITEM
};

Sorter SORTER = &SortByCode;

} // namespace

Genre Genre::Load(DB::IDatabase& db)
{
	using AllGenresItem = std::tuple<Genre, QString>;
	std::unordered_map<QString, AllGenresItem> allGenres;
	std::vector<AllGenresItem> buffer;
	const auto query = db.CreateQuery("select g.GenreCode, g.FB2Code, g.ParentCode, g.GenreAlias, (select count(42) from Genre_List gl where gl.GenreCode = g.GenreCode) BookCount, IsDeleted from Genres g");
	for (query->Execute(); !query->Eof(); query->Next())
	{
		const auto* fb2Code = query->Get<const char*>(1);
		auto translated = Loc::Tr(GENRE, fb2Code);
		assert(!translated.contains(','));
		AllGenresItem item {
			Genre { .fb2Code = fb2Code,
                   .code = query->Get<const char*>(0),
                   .name = translated != fb2Code ? std::move(translated) : query->Get<const char*>(3),
                   .removed = static_cast<bool>(query->Get<int>(5)) },
			query->Get<const char*>(2)
		};
		if (query->Get<int>(4))
			buffer.emplace_back(std::move(item));
		else
			allGenres.try_emplace(query->Get<const char*>(0), std::move(item));
	}

	const auto updateChildren = [](auto& childrenList)
	{
		std::ranges::sort(childrenList, {}, [](const auto& item) { return item.code; });
		for (size_t i = 0, sz = childrenList.size(); i < sz; ++i)
			childrenList[i].row = i;
	};

	Genre root;
	while (!buffer.empty())
	{
		for (auto&& [genre, parentCode] : buffer)
		{
			updateChildren(genre.children);
			const auto it = allGenres.find(parentCode);
			(it == allGenres.end() ? root : std::get<0>(it->second)).children.emplace_back(std::move(genre));
		}
		buffer.clear();

		for (auto&& genre : allGenres | std::views::values | std::views::filter([](const auto& item) { return !get<0>(item).children.empty(); }))
			buffer.emplace_back(std::move(genre));
		for (const auto& [genre, _] : buffer)
			allGenres.erase(genre.code);
	}

	updateChildren(root.children);

	SORTER(root);
	return root;
}

Genre* Genre::Find(Genre* root, const QString& code)
{
	if (root->code == code)
		return root;

	for (auto& child : root->children)
		if (auto* found = Find(&child, code))
			return found;

	return nullptr;
}

void Genre::SetSortMode(const ISettings& settings)
{
	SORTER = FindSecond(SORTERS, settings.Get(GENRES_SORT_MODE_KEY).toString().toStdString().data(), &SortByCode, PszComparer {});
}
