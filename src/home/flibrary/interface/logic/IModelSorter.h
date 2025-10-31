#pragma once

namespace HomeCompa::Flibrary
{

class IModelSorter // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IModelSorter() = default;

	virtual bool LessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight, int emptyStringWeight) const = 0;
};

} // namespace HomeCompa::Flibrary
