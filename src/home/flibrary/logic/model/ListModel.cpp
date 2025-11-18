#include "ListModel.h"

#include "fnd/algorithm.h"

#include "interface/constants/ModelRole.h"

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

QVariant ListModel::data(const QModelIndex& index, const int role) const
{
	return role == Role::IsTree ? QVariant { false } : AbstractListModel::data(index, role);
}

bool ListModel::Check(const QVariant& value, const Qt::CheckState checked)
{
	const auto    toChange = value.value<std::set<QString>>();
	std::set<int> changed;
	for (size_t i = 0, sz = m_data->GetChildCount(); i < sz; ++i)
	{
		if (auto item = m_data->GetChild(i); item->GetCheckState() != checked && toChange.contains(item->GetId()))
		{
			item->SetCheckState(checked);
			changed.insert(static_cast<int>(i));
		}
	}

	for (const auto& [begin, end] : Util::CreateRanges(changed))
		emit dataChanged(index(begin, 0), index(end - 1, 0), {Qt::CheckStateRole});

	return !changed.empty();
}
