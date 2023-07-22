#include "ListModel.h"

#include <plog/Log.h>

#include "data/AbstractModelProvider.h"

using namespace HomeCompa::Flibrary;

AbstractListModel::AbstractListModel(const std::shared_ptr<AbstractModelProvider> & modelProvider, QObject * parent)
	: BaseModel(modelProvider, parent)
{
}

ListModel::ListModel(const std::shared_ptr<AbstractModelProvider> & modelProvider, QObject * parent)
	: AbstractListModel(modelProvider, parent)
{
	PLOGD << "ListModel created";
}

ListModel::~ListModel()
{
	PLOGD << "ListModel destroyed";
}

QModelIndex ListModel::index(const int row, const int column, const QModelIndex & parent) const
{
	return hasIndex(row, column, parent)
		? createIndex(row, column, m_data->GetChild(row))
		: QModelIndex();
}

QModelIndex ListModel::parent(const QModelIndex & /*index*/) const
{
	return {};
}

int ListModel::rowCount(const QModelIndex & parent) const
{
	return parent.isValid() ? 0 : static_cast<int>(m_data->GetChildCount());
}

int ListModel::columnCount(const QModelIndex & /*parent*/) const
{
	return 1;
}
