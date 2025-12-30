#include "TreeModel.h"

#include "fnd/algorithm.h"

#include "interface/constants/ModelRole.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

AbstractTreeModel::AbstractTreeModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent)
	: BaseModel(modelProvider, parent)
{
}

TreeModel::TreeModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent)
	: AbstractTreeModel(modelProvider, parent)
{
	PLOGV << "TreeModel created";
}

TreeModel::~TreeModel()
{
	PLOGV << "TreeModel destroyed";
}

QModelIndex TreeModel::index(const int row, const int column, const QModelIndex& parent) const
{
	if (!hasIndex(row, column, parent))
		return {};

	const auto* parentItem = parent.isValid() ? static_cast<IDataItem*>(parent.internalPointer()) : m_data.get();
	const auto  childItem  = parentItem->GetChild(static_cast<size_t>(row));

	return childItem ? createIndex(row, column, childItem.get()) : QModelIndex();
}

QModelIndex TreeModel::parent(const QModelIndex& index) const
{
	if (!index.isValid())
		return {};

	const auto* childItem  = static_cast<IDataItem*>(index.internalPointer());
	const auto* parentItem = childItem->GetParent();

	return parentItem != m_data.get() ? createIndex(static_cast<int>(parentItem->GetRow()), 0, parentItem) : QModelIndex();
}

int TreeModel::rowCount(const QModelIndex& parent) const
{
	const auto* parentItem = parent.isValid() ? static_cast<IDataItem*>(parent.internalPointer()) : m_data.get();
	return static_cast<int>(parentItem->GetChildCount());
}

int TreeModel::columnCount(const QModelIndex& /*parent*/) const
{
	return m_data->GetColumnCount();
}

QVariant TreeModel::data(const QModelIndex& index, const int role) const
{
	return role == Role::IsTree ? QVariant { true } : AbstractTreeModel::data(index, role);
}

bool TreeModel::Check(const QVariant& value, const Qt::CheckState checked)
{
	const auto toChange = value.value<std::set<QString>>();

	const auto enumerate = [&](const QModelIndex& parent, const auto& r) -> bool {
		std::set<int> changed;

		for (int i = 0, sz = rowCount(parent); i < sz; ++i)
		{
			const auto child = index(i, 0, parent);
			if (rowCount(child))
			{
				if (r(child, r))
					changed.insert(i);
			}
			else if (child.data(Role::CheckState).value<Qt::CheckState>() != checked && toChange.contains(child.data(Role::Id).toString()))
			{
				setData(child, checked, Role::CheckState);
				changed.insert(i);
			}
		}

		for (const auto& [begin, end] : Util::CreateRanges(changed))
			emit dataChanged(index(begin, 0, parent), index(end - 1, 0, parent), { Qt::CheckStateRole });

		return !changed.empty();
	};

	return enumerate({}, enumerate);
}
