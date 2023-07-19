#pragma once

#include "Types.h"

class QAbstractItemModel;

namespace HomeCompa::Flibrary {

class AbstractModelProvider  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~AbstractModelProvider() = default;

public:
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel> CreateListModel(DataItem::Ptr data, class IModelObserver & observer) const = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel> CreateTreeModel(DataItem::Ptr data, class IModelObserver & observer) const = 0;
	[[nodiscard]] virtual DataItem::Ptr GetData() const noexcept = 0;
	[[nodiscard]] virtual IModelObserver & GetObserver() const noexcept = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel> GetSourceModel() const noexcept = 0;
};

}
