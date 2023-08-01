#include "FilteredProxyModel.h"

#include <set>
#include <stack>

#include "interface/constants/ModelRole.h"

#include "data/AbstractModelProvider.h"
#include "interface/constants/Enums.h"

using namespace HomeCompa::Flibrary;

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
	std::stack<QModelIndex> stack { {QModelIndex{}} };
	while (!stack.empty())
	{
		const auto parent = stack.top();
		stack.pop();
		for (int i = 0, sz = rowCount(parent); i < sz; ++i)
		{
			const auto child = index(i, 0, parent);
			if (child.data(Role::Type).value<ItemType>() == ItemType::Navigation)
			{
				stack.push(child);
				continue;
			}

			assert(child.data(Role::Type).value<ItemType>() == ItemType::Books);
			languages.insert(child.data(Role::Lang).toString().toLower());
		}
	}

	return { languages.cbegin(), languages.cend() };
}
