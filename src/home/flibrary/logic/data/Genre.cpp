#include "Genre.h"

#include <ranges>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/GenresLocalization.h"
#include "interface/constants/Localization.h"

#include "util/ISettings.h"
#include "util/localization.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto GENRES_SORT_MODE_KEY = "ui/GenresSortMode";

template <typename T>
void Sort(T& root, const auto& proj)
{
	std::ranges::sort(root.children, {}, proj);
	for (auto& child : root.children)
		Sort(child, proj);
}

template <typename T>
void SortDesc(T& root, const auto& proj)
{
	std::ranges::sort(root.children, std::greater {}, proj);
	for (auto& child : root.children)
		Sort(child, proj);
}

template <typename T>
void SortByCode(T& root)
{
	Sort<T>(root, [](const auto& item) { return item.code; });
}

template <typename T>
void SortByChildCount(T& root)
{
	Sort(root, [](const auto& item) { return item.children.size(); });
}

template <typename T>
void SortByName(T& root)
{
	Sort(root, [](const auto& item) { return item.name; });
}

template <typename T>
void SortByNameIntegral(T& root)
{
	Sort(root, [](const auto& item) { return item.name.toLongLong(); });
}

template <typename T>
void SortByCodeDesc(T& root)
{
	SortDesc(root, [](const auto& item) { return item.code; });
}

template <typename T>
void SortByNameDesc(T& root)
{
	SortDesc(root, [](const auto& item) { return item.name; });
}

template <typename T>
void SortByChildCountDesc(T& root)
{
	SortDesc(root, [](const auto& item) { return item.children.size(); });
}

template <typename T>
void SortStub(T&)
{
}

template <typename T>
using Sorter = void (*)(T&);
constexpr std::pair<const char*, Sorter<Genre>> SORTERS[] {
#define ITEM(NAME) { #NAME, &NAME<Genre> } // NOLINT(bugprone-macro-parentheses)
	ITEM(SortByCode), ITEM(SortByName), ITEM(SortByChildCount), ITEM(SortByCodeDesc), ITEM(SortByNameDesc), ITEM(SortByChildCountDesc),
#undef ITEM
};

Sorter<Genre> SORTER = &SortByCode;

template <typename T>
using AllTreeItem = std::tuple<T, typename T::CodeType>;

template <typename T>
void Select(DB::IQuery& query, const std::unordered_set<typename T::CodeType>& neededItems, std::unordered_map<typename T::CodeType, AllTreeItem<T>>& allItems, std::vector<AllTreeItem<T>>& buffer) = delete;

template <>
void Select<Genre>(DB::IQuery& query, const std::unordered_set<Genre::CodeType>& neededItems, std::unordered_map<Genre::CodeType, AllTreeItem<Genre>>& allItems, std::vector<AllTreeItem<Genre>>& buffer)
{
	const auto* fb2Code = query.Get<const char*>(1);
	auto translated = Loc::Tr(GENRE, fb2Code);
	assert(!translated.contains(','));
	AllTreeItem<Genre> item {
		Genre { .fb2Code = fb2Code,
               .code = query.Get<const char*>(0),
               .name = translated != fb2Code ? std::move(translated) : query.Get<const char*>(3),
               .removed = static_cast<bool>(query.Get<int>(5)),
               .flags = static_cast<IDataItem::Flags>(query.Get<int>(6)) },
		query.Get<const char*>(2)
	};
	if (query.Get<int>(4) && (neededItems.empty() || neededItems.contains(std::get<0>(item).code)))
		buffer.emplace_back(std::move(item));
	else
		allItems.try_emplace(std::get<0>(item).code, std::move(item));
}

template <>
void Select<Update>(DB::IQuery& query, const std::unordered_set<Update::CodeType>& neededItems, std::unordered_map<Update::CodeType, AllTreeItem<Update>>& allItems, std::vector<AllTreeItem<Update>>& buffer)
{
	AllTreeItem<Update> item {
		Update { .code = query.Get<long long>(0), .name = query.Get<const char*>(1), .removed = static_cast<bool>(query.Get<int>(4)), .flags = static_cast<IDataItem::Flags>(query.Get<int>(5)) },
		query.Get<long long>(2)
	};
	if (query.Get<int>(3) && (neededItems.empty() || neededItems.contains(std::get<0>(item).code)))
		buffer.emplace_back(std::move(item));
	else
		allItems.try_emplace(std::get<0>(item).code, std::move(item));
}

template <typename T>
T LoadImpl(DB::IDatabase& db, const std::unordered_set<typename T::CodeType>& neededItems, const std::string_view queryText, const Sorter<T> sorter = &SortStub<T>)
{
	std::unordered_map<typename T::CodeType, AllTreeItem<T>> allItems;
	std::vector<AllTreeItem<T>> buffer;
	const auto query = db.CreateQuery(queryText);
	for (query->Execute(); !query->Eof(); query->Next())
		Select<T>(*query, neededItems, allItems, buffer);

	T root;
	while (!buffer.empty())
	{
		for (auto&& [treeItem, parentCode] : buffer)
		{
			const auto it = allItems.find(parentCode);
			(it == allItems.end() ? root : std::get<0>(it->second)).children.emplace_back(std::move(treeItem));
		}
		buffer.clear();

		for (auto&& treeItem : allItems | std::views::values | std::views::filter([](const auto& item) { return !get<0>(item).children.empty(); }))
			buffer.emplace_back(std::move(treeItem));
		for (const auto& [treeItem, _] : buffer)
			allItems.erase(treeItem.code);
	}

	sorter(root);

	const auto updateParent = [](T& treeItem, const auto& f) -> void
	{
		for (size_t row = 0; auto& child : treeItem.children)
		{
			child.parent = &treeItem;
			child.row = row++;
			f(child, f);
		}
	};
	updateParent(root, updateParent);

	return root;
}

template <typename T>
T* FindImpl(T* root, const typename T::CodeType& code)
{
	if (root->code == code)
		return root;

	for (auto& child : root->children)
		if (auto* found = FindImpl<T>(&child, code))
			return found;

	return nullptr;
}

} // namespace

Genre Genre::Load(DB::IDatabase& db, const std::unordered_set<QString>& neededGenres)
{
	return LoadImpl<Genre>(db,
	                       neededGenres,
	                       "select g.GenreCode, g.FB2Code, g.ParentCode, g.GenreAlias, exists (select 42 from Genre_List gl where gl.GenreCode = g.GenreCode) BookCount, IsDeleted, Flags from Genres g",
	                       SORTER);
}

Update Update::Load(DB::IDatabase& db, const std::unordered_set<long long>& neededUpdates)
{
	auto root = LoadImpl<Update>(db,
	                             neededUpdates,
	                             "select u.UpdateId, u.UpdateTitle, u.ParentId, exists (select 42 from Books b where b.UpdateID = u.UpdateID) BookCount, u.IsDeleted, 0 Flags from Updates u",
	                             &SortByNameIntegral<Update>);

	const auto tr = [](Update& treeItem, const auto& f) -> void
	{
		for (auto& child : treeItem.children)
		{
			child.name = Loc::Tr(MONTHS_CONTEXT, child.name.toStdString().data());
			f(child, f);
		}
	};
	tr(root, tr);
	return root;
}

Genre* Genre::Find(Genre* root, const QString& code)
{
	return FindImpl(root, code);
}

Update* Update::Find(Update* root, const long long code)
{
	return FindImpl(root, code);
}

void Genre::SetSortMode(const ISettings& settings)
{
	SORTER = FindSecond(SORTERS, settings.Get(GENRES_SORT_MODE_KEY).toString().toStdString().data(), &SortByCode, PszComparer {});
}
