#include "ListModel.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

AbstractListModel::AbstractListModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent)
	: BaseModel(modelProvider, parent)
{
}

ListModel::ListModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent)
	: AbstractListModel(modelProvider, parent)
{
	PLOGV << "ListModel created";
}

ListModel::~ListModel()
{
	PLOGV << "ListModel destroyed";
}

QModelIndex ListModel::index(const int row, const int column, const QModelIndex& parent) const
{
	return hasIndex(row, column, parent) ? createIndex(row, column, m_data->GetChild(static_cast<size_t>(row)).get()) : QModelIndex();
}

QModelIndex ListModel::parent(const QModelIndex& /*index*/) const
{
	return {};
}

int ListModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : static_cast<int>(m_data->GetChildCount());
}

int ListModel::columnCount(const QModelIndex& parent) const
{
	const auto* parentItem = parent.isValid() ? static_cast<IDataItem*>(parent.internalPointer()) : m_data.get();
	return parentItem->GetChildCount() ? parentItem->GetChild(0)->GetColumnCount() : 0;
}
