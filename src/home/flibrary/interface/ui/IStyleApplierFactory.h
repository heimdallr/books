#pragma once

#include <vector>

#include "IStyleApplier.h"

class QAction;

namespace HomeCompa::Flibrary
{

class IStyleApplierFactory // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	static constexpr auto ACTION_PROPERTY_NAME = "value";
	static constexpr auto ACTION_PROPERTY_TYPE = "type";
	static constexpr auto ACTION_PROPERTY_DATA = "data";

public:
	virtual ~IStyleApplierFactory() = default;
	virtual std::shared_ptr<IStyleApplier> CreateStyleApplier(IStyleApplier::Type type) const = 0;
	virtual void CheckAction(const std::vector<QAction*>& actions) const = 0;
};

}
