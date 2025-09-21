#include "FilterListModel.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

FilterListModel::FilterListModel(const std::shared_ptr<IModelProvider>& modelProvider, std::shared_ptr<IFilterController> controller, QObject* parent)
	: FilterModel(modelProvider, std::move(controller), parent)
{
	PLOGV << "FilterListModel created";
}

FilterListModel::~FilterListModel()
{
	PLOGV << "FilterListModel destroyed";
}

Qt::ItemFlags FilterListModel::flags(const QModelIndex& index) const
{
	auto flags = FilterModel::flags(index);
	if (index.column() > 0)
		flags |= Qt::ItemFlag::ItemIsUserCheckable | Qt::ItemFlag::ItemIsEditable;
	return flags;
}
