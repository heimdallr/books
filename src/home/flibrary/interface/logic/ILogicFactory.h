#pragma once

#include <memory>
#include <string>

#include "fnd/observer.h"

namespace HomeCompa::DB {
class IDatabase;
}

namespace HomeCompa::Util {
struct ExecutorInitializer;
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
	[[nodiscard]] virtual std::shared_ptr<class AbstractTreeViewController> CreateTreeViewController(enum class TreeViewControllerType type) const = 0;
	[[nodiscard]] virtual std::unique_ptr<DB::IDatabase> GetDatabase() const = 0;
	[[nodiscard]] virtual std::unique_ptr<Util::IExecutor> GetExecutor(Util::ExecutorInitializer initializer) const = 0;
};

}
