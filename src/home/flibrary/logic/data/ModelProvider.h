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
	[[nodiscard]] std::shared_ptr<QAbstractItemModel> CreateListModel(IDataItem::Ptr data, IModelObserver & observer) const override;
	[[nodiscard]] std::shared_ptr<QAbstractItemModel> CreateTreeModel(IDataItem::Ptr data, IModelObserver & observer) const override;
	[[nodiscard]] IDataItem::Ptr GetData() const noexcept override;
	[[nodiscard]] IModelObserver & GetObserver() const noexcept override;
	[[nodiscard]] std::shared_ptr<QAbstractItemModel> GetSourceModel() const noexcept override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
