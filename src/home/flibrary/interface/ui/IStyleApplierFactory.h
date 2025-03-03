#pragma once

#include <vector>

#include "IStyleApplier.h"

class QAction;

namespace HomeCompa::Flibrary
{

class IStyleApplierFactory // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IStyleApplierFactory() = default;
	virtual std::shared_ptr<IStyleApplier> CreateStyleApplier(IStyleApplier::Type type) const = 0;
	virtual std::shared_ptr<IStyleApplier> CreateThemeApplier() const = 0;
	virtual void CheckAction(const std::vector<QAction*>& actions) const = 0;
};

}
