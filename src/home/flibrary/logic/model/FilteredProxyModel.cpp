#include "FilteredProxyModel.h"

#include <set>
#include <stack>

#include "interface/constants/ModelRole.h"

#include "data/AbstractModelProvider.h"
#include "interface/constants/Enums.h"

using namespace HomeCompa::Flibrary;

namespace {

void EnumerateLeafs(const QAbstractItemModel & model, const QModelIndexList & indexList, const std::function<void(const QModelIndex&)> & f)
{
	std::vector<QModelIndex> stack;
	std::ranges::copy_if(indexList, std::back_inserter(stack), [] (const QModelIndex & index) { return index.column() == 0; });

	while (!stack.empty())
	{
		const auto parent = stack.back();
		stack.pop_back();
		const auto rowCount = model.rowCount(parent);
		if (rowCount == 0)
			f(parent);

		for (int i = 0; i < rowCount; ++i)
			stack.push_back(model.index(i, 0, parent));
	}
}

}

AbstractFilteredProxyModel::AbstractFilteredProxyModel(QObject * parent)
	: QIdentityProxyModel(parent)
{
}

FilteredProxyModel::FilteredProxyModel(const std::shared_ptr<class AbstractModelProvider> & modelProvider, QObject * parent)
	: AbstractFilteredProxyModel(parent)
	, m_sourceModel(modelProvider->GetSourceModel())
{
	QIdentityProxyModel::setSourceModel(m_sourceModel.get());
}

FilteredProxyModel::~FilteredProxyModel() = default;

QVariant FilteredProxyModel::data(const QModelIndex & index, const int role) const
{
	if (index.isValid())
		return QIdentityProxyModel::data(index, role);

	switch(role)
	{
		case Role::Languages:
			return CollectLanguages();

		case Role::Count:
			return GetCount();

		default:
			break;
	}

	return QIdentityProxyModel::data(index, role);
}

bool FilteredProxyModel::setData(const QModelIndex & index, const QVariant & value, const int role)
{
	if (index.isValid())
	{
		switch (role)
		{
			case Qt::CheckStateRole:
				if (const auto checkState = index.data(Qt::CheckStateRole); checkState.isValid())
				{
					Check(index, checkState.value<Qt::CheckState>() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
					auto parent = index;
					while (parent.isValid())
					{
						emit dataChanged(parent, parent, { Qt::CheckStateRole });
						parent = parent.parent();
					}
					return true;
				}

				return false;

			default:
				break;
		}
	}
	else
	{
		switch (role)
		{
			case Role::Selected:
			{
				const auto request = value.value<SelectedRequest>();
				GetSelected(request.current, request.selected, request.result);
				return true;
			}

			default:
				break;
		}
	}

	return m_sourceModel->setData(mapToSource(index), value, role);
}


void FilteredProxyModel::Check(const QModelIndex & parent, const Qt::CheckState state)
{
	const auto count = rowCount(parent);
	for (auto i = 0; i < count; ++i)
		Check(index(i, 0, parent), state);

	emit dataChanged(index(0, 0, parent), index(count - 1, 0), { Qt::CheckStateRole });

	setData(parent, state, Role::CheckState);
}

QStringList FilteredProxyModel::CollectLanguages() const
{
	std::set<QString> languages;
	EnumerateLeafs(*this, {QModelIndex{}}, [&] (const QModelIndex & child)
	{
		if (child.data(Role::Type).value<ItemType>() == ItemType::Books)
			languages.insert(child.data(Role::Lang).toString().toLower());
	});

	return { languages.cbegin(), languages.cend() };
}

int FilteredProxyModel::GetCount() const
{
	int result = 0;
	EnumerateLeafs(*this, { QModelIndex{} }, [&] (const QModelIndex & child)
	{
		if (child.data(Role::Type).value<ItemType>() == ItemType::Books)
			++result;
	});

	return result;
}

void FilteredProxyModel::GetSelected(const QModelIndex & index, const QModelIndexList & indexList, QModelIndexList * selected) const
{
	EnumerateLeafs(*this, { QModelIndex{} }, [&] (const QModelIndex & child)
	{
		if (child.data(Role::Type).value<ItemType>() == ItemType::Books && child.data(Role::CheckState).value<Qt::CheckState>() == Qt::Checked)
			(*selected) << child;
	});

	if (selected->isEmpty())
		EnumerateLeafs(*this, indexList, [&] (const QModelIndex & child)
	{
		if (child.data(Role::Type).value<ItemType>() == ItemType::Books)
			(*selected) << child;
	});

	if (selected->isEmpty())
		(*selected) << index;
}
