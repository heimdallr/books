#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "AbstractModelProvider.h"

class QAbstractItemModel;

namespace Hypodermic {
class Container;
}

namespace HomeCompa::Flibrary {

class ModelProvider final : virtual public AbstractModelProvider
{
	NON_COPY_MOVABLE(ModelProvider)

public:
	explicit ModelProvider(Hypodermic::Container & container);
	~ModelProvider() override;

public:
	[[nodiscard]] std::shared_ptr<QAbstractItemModel> CreateModel(DataItem::Ptr data, IModelObserver & observer) const override;
	[[nodiscard]] DataItem::Ptr GetData() const noexcept override;
	[[nodiscard]] IModelObserver & GetObserver() const noexcept override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
