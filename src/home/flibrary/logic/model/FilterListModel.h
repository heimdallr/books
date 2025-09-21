#pragma once

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IFilterController.h"
#include "interface/logic/IModelProvider.h"

#include "FilterModel.h"
#include "ListModel.h"

namespace HomeCompa::Flibrary
{

class FilterListModel : public FilterModel<ListModel>
{
	NON_COPY_MOVABLE(FilterListModel)

public:
	FilterListModel(const std::shared_ptr<IModelProvider>& modelProvider, std::shared_ptr<IFilterController> controller, QObject* parent = nullptr);
	~FilterListModel() override;

private: // QAbstractItemModel
	Qt::ItemFlags flags(const QModelIndex& index) const override;
};

} // namespace HomeCompa::Flibrary
