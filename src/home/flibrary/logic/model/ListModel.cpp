#include "ListModel.h"

#include <plog/Log.h>

#include "interface/constants/ModelRole.h"

#include "data/AbstractModelProvider.h"

using namespace HomeCompa::Flibrary;

AbstractListModel::AbstractListModel(QObject * parent)
	: QAbstractItemModel(parent)
{
}

ListModel::ListModel(const std::shared_ptr<AbstractModelProvider> & modelProvider, QObject * parent)
	: AbstractListModel(parent)
	, m_data(modelProvider->GetData())
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

QVariant ListModel::data(const QModelIndex & index, const int role) const
{
	if (!index.isValid())
		return {};

	const auto * item = static_cast<DataItem *>(index.internalPointer());
	switch (role)
	{
		case Qt::DisplayRole:
			return item->GetData(index.column());

		case Role::Id:
			return item->GetId();

		default:
			break;
	}

	return {};
}

