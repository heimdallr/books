#include "Genre.h"

#include <QHash>

#include <ranges>

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/GenresLocalization.h"

#include "inpx/src/util/constant.h"
#include "util/localization.h"

using namespace HomeCompa::Flibrary;

Genre Genre::Load(DB::IDatabase& db, const bool showDateAdded)
{
	using AllGenresItem = std::tuple<Genre, QString>;
	std::unordered_map<QString, AllGenresItem> allGenres;
	std::vector<AllGenresItem> buffer;
	const auto query = db.CreateQuery("select g.GenreCode, g.FB2Code, g.ParentCode, g.GenreAlias, (select count(42) from Genre_List gl where gl.GenreCode = g.GenreCode) BookCount from Genres g");
	for (query->Execute(); !query->Eof(); query->Next())
	{
		const auto* fb2Code = query->Get<const char*>(1);
		auto translated = Loc::Tr(GENRE, fb2Code);
		AllGenresItem item {
			Genre { .code = query->Get<const char*>(0), .name = translated != fb2Code ? std::move(translated) : query->Get<const char*>(3) },
			query->Get<const char*>(2)
		};
		if (query->Get<int>(4))
			buffer.emplace_back(std::move(item));
		else
			allGenres.try_emplace(query->Get<const char*>(0), std::move(item));
	}

	const auto updateChildren = [](auto& children)
	{
		std::ranges::sort(children, {}, [](const auto& item) { return item.code; });
		for (int i = 0, sz = static_cast<int>(children.size()); i < sz; ++i)
			children[i].row = i;
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

	if (!showDateAdded)
		std::erase_if(root.children, [dateAddedCode = Loc::Tr(GENRE, QString::fromStdWString(DATE_ADDED_CODE).toStdString().data())](const Genre& item) { return item.name == dateAddedCode; });

	updateChildren(root.children);

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
