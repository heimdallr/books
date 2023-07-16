#pragma once

#include <memory>
#include <string>

#include "fnd/observer.h"

namespace HomeCompa::DB {
class IDatabase;
}

namespace HomeCompa::DB::Factory {
enum class Impl;
}

namespace HomeCompa::Flibrary {

class ILogicFactory  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		virtual void OnDatabaseChanged(std::shared_ptr<DB::IDatabase> db) = 0;
	};

public:
	virtual ~ILogicFactory() = default;

public:
	[[nodiscard]] virtual std::shared_ptr<class AbstractTreeViewController> CreateTreeViewController(enum class TreeViewControllerType type) const = 0;
	virtual void SetDatabase(std::shared_ptr<DB::IDatabase> db) = 0;
	[[nodiscard]] virtual std::shared_ptr<DB::IDatabase> GetDatabase() const = 0;

	virtual void RegisterObserver(IObserver * observer) = 0;
	virtual void UnregisterObserver(IObserver * observer) = 0;
};

}
