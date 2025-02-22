#pragma once

#include <functional>
#include <unordered_set>

class QString;

namespace HomeCompa::Flibrary
{

class IBookSearchController // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Callback = std::function<void(long long)>;
	using Id = long long;
	using Ids = std::unordered_set<Id>;

	struct SearchMode
	{
#define SEARCH_MODE_ITEMS_X_MACRO \
	SEARCH_MODE_ITEM(Contains)    \
	SEARCH_MODE_ITEM(StartsWith)  \
	SEARCH_MODE_ITEM(EndsWith)    \
	SEARCH_MODE_ITEM(Equals)

		enum
		{

#define SEARCH_MODE_ITEM(NAME) NAME,
			SEARCH_MODE_ITEMS_X_MACRO
#undef SEARCH_MODE_ITEM
		};
	};

public:
	virtual ~IBookSearchController() = default;

public:
	virtual void CreateNew(Callback callback) = 0;
	virtual void Remove(Ids ids, Callback callback) const = 0;
	virtual void Search(QString searchString, Callback callback, int mode = SearchMode::Contains) = 0;
};

} // namespace HomeCompa::Flibrary
