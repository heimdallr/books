#pragma once

#include <memory>

#include "util/executor/factory.h"

namespace HomeCompa::DB {
class IDatabase;
}

namespace HomeCompa::Util {
class IExecutor;
}

namespace HomeCompa::DB::Factory {
enum class Impl;
}

namespace HomeCompa::Flibrary {

class ILogicFactory  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ILogicFactory() = default;

public:
	[[nodiscard]] virtual std::shared_ptr<class AbstractTreeViewController> GetTreeViewController(enum class ItemType type) const = 0;
	[[nodiscard]] virtual std::shared_ptr<class ArchiveParser> CreateArchiveParser() const = 0;
	[[nodiscard]] virtual std::unique_ptr<Util::IExecutor> GetExecutor(Util::ExecutorInitializer initializer = {}) const = 0;
	[[nodiscard]] virtual std::shared_ptr<class GroupController> CreateGroupController() const = 0;
};

}
