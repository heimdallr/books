#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/logic/ILogicFactory.h"

namespace Hypodermic {
class Container;
}

namespace HomeCompa::Flibrary {

class LogicFactory final : virtual public ILogicFactory
{
	NON_COPY_MOVABLE(LogicFactory)

public:
	explicit LogicFactory(Hypodermic::Container & container);
	~LogicFactory() override;

private: // ILogicFactory
	[[nodiscard]] std::shared_ptr<AbstractTreeViewController> CreateTreeViewController(TreeViewControllerType type) const override;
	void SetDatabase(std::shared_ptr<DB::IDatabase> db) override;
	[[nodiscard]] std::shared_ptr<DB::IDatabase> GetDatabase() const override;

	void RegisterObserver(IObserver * observer) override;
	void UnregisterObserver(IObserver * observer) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
