#pragma once

#include <qmetatype.h>

namespace HomeCompa::Flibrary
{

class IModelSorter // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IModelSorter() = default;

	virtual bool LessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight, int emptyStringWeight) const = 0;
};

} // namespace HomeCompa::Flibrary

Q_DECLARE_METATYPE(const HomeCompa::Flibrary::IModelSorter*);
