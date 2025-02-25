#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/ui/IStyleApplierFactory.h"

namespace Hypodermic
{
class Container;
}

namespace HomeCompa::Flibrary
{

class StyleApplierFactory : virtual public IStyleApplierFactory
{
	NON_COPY_MOVABLE(StyleApplierFactory)

public:
	explicit StyleApplierFactory(Hypodermic::Container& container);
	~StyleApplierFactory() override;

private: // IStyleApplierFactory
	std::shared_ptr<IStyleApplier> CreateStyleApplier(IStyleApplier::Type type) const override;
	void CheckAction(const std::vector<QAction*>& actions) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
