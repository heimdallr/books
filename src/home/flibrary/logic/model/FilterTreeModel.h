#pragma once

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IFilterController.h"
#include "interface/logic/IModelProvider.h"

#include "FilterModel.h"
#include "TreeModel.h"

namespace HomeCompa::Flibrary
{

class FilterTreeModel : public FilterModel<TreeModel>
{
	NON_COPY_MOVABLE(FilterTreeModel)

public:
	FilterTreeModel(const std::shared_ptr<IModelProvider>& modelProvider, std::shared_ptr<IFilterController> controller, QObject* parent = nullptr);
	~FilterTreeModel() override;

private: // QAbstractItemModel
	Qt::ItemFlags flags(const QModelIndex& index) const override;

private: // FilterModel
	std::vector<IDataItem::Ptr> GetHideFilteredChanged(const std::unordered_set<QString>& ids) const override;
	QVariant                    IsChecked(const IDataItem& item, IDataItem::Flags flags) const override;
};

} // namespace HomeCompa::Flibrary
