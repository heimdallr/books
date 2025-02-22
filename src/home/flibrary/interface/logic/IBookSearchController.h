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

public:
	virtual ~IBookSearchController() = default;

public:
	virtual void CreateNew(Callback callback) = 0;
	virtual void Remove(Ids ids, Callback callback) const = 0;
	virtual void Search(QString searchString, Callback callback) = 0;
};

} // namespace HomeCompa::Flibrary
