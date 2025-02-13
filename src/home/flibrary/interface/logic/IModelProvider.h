#pragma once

#include "fnd/Lockable.h"

#include "interface/logic/IDataItem.h"

class QAbstractItemModel;

namespace HomeCompa::Flibrary
{

class IModelProvider : public Lockable<IModelProvider> // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IModelProvider() = default;

public:
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel> CreateListModel(IDataItem::Ptr data, class IModelObserver& observer) const = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel> CreateTreeModel(IDataItem::Ptr data, class IModelObserver& observer) const = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel> CreateScriptModel() const = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel> CreateScriptCommandModel() const = 0;
	[[nodiscard]] virtual IDataItem::Ptr GetData() const noexcept = 0;
	[[nodiscard]] virtual IModelObserver& GetObserver() const noexcept = 0;
	[[nodiscard]] virtual std::shared_ptr<QAbstractItemModel> GetSourceModel() const noexcept = 0;
};

}
