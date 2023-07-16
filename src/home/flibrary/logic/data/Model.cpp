#include "Model.h"

#include "AbstractModelProvider.h"

using namespace HomeCompa::Flibrary;

AbstractTreeModel::AbstractTreeModel(QObject * parent)
	: QAbstractItemModel(parent)
{
}

TreeModel::TreeModel(const std::shared_ptr<AbstractModelProvider> & modelProvider, QObject * parent)
	: AbstractTreeModel(parent)
	, m_data(modelProvider->GetData())
{
}

TreeModel::~TreeModel() = default;

QModelIndex TreeModel::index(int /*row*/, int /*column*/, const QModelIndex & /*parent*/) const
{
	return {};
}

QModelIndex TreeModel::parent(const QModelIndex & /*child*/) const
{
	return {};
}

int TreeModel::rowCount(const QModelIndex & /*parent*/) const
{
	return {};
}

int TreeModel::columnCount(const QModelIndex & /*parent*/) const
{
	return {};
}

QVariant TreeModel::data(const QModelIndex & /*index*/, int /*role*/) const
{
	return {};
}

