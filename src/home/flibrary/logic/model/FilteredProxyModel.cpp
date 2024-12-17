#include "FilteredProxyModel.h"

#include <set>
#include <queue>

#include "interface/constants/ModelRole.h"
#include "interface/constants/Enums.h"
#include "interface/logic/IModelProvider.h"

using namespace HomeCompa::Flibrary;

namespace {

void EnumerateLeafs(const QAbstractItemModel & model, const QModelIndexList & indexList, const std::function<void(const QModelIndex&)> & f)
{
	std::set<QString> processed;

	std::queue<QModelIndex> queue;
	for (const auto & index : indexList)
		if (!index.isValid() || index.column() == 0)
			queue.push(index);

	while (!queue.empty())
	{
		const auto parent = queue.front();
		queue.pop();
		const auto rowCount = model.rowCount(parent);
		if (parent.isValid() && rowCount == 0 && processed.emplace(parent.data(Role::Id).toString()).second)
			f(parent);

		for (int i = 0; i < rowCount; ++i)
			queue.push(model.index(i, 0, parent));
	}
}

}

AbstractFilteredProxyModel::AbstractFilteredProxyModel(QObject * parent)
	: QIdentityProxyModel(parent)
{
}

FilteredProxyModel::FilteredProxyModel(const std::shared_ptr<class IModelProvider> & modelProvider, QObject * parent)
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
		if (role != Qt::CheckStateRole)
			return QIdentityProxyModel::setData(index, value, role);

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
	}

	switch (role)
	{
		case Role::Selected:
		{
			const auto request = value.value<SelectedRequest>();
			GetSelected(request.current, request.selected, request.result);
			return true;
		}

		case Role::CheckAll:
			return Check(value, Qt::Checked);

		case Role::UncheckAll:
			return Check(value, Qt::Unchecked);

		case Role::InvertCheck:
			return Check(value, [&] (const QModelIndex & bookIndex)
			{
				return setData(bookIndex, bookIndex.data(Qt::CheckStateRole).value<Qt::CheckState>() == Qt::Checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
			});

		default:
			break;
	}

	return QIdentityProxyModel::setData(index, value, role);
}

bool FilteredProxyModel::Check(const QVariant & value, const Qt::CheckState checkState)
{
	return Check(value, [&] (const QModelIndex & index)
	{
		return true
			&& index.data(Qt::CheckStateRole).value<Qt::CheckState>() != checkState
			&& setData(index, checkState, Qt::CheckStateRole)
			;
	});
}

void FilteredProxyModel::Check(const QModelIndex & parent, const Qt::CheckState state)
{
	const auto count = rowCount(parent);
	for (auto i = 0; i < count; ++i)
		Check(index(i, 0, parent), state);

	emit dataChanged(index(0, 0, parent), index(count - 1, 0), { Qt::CheckStateRole });

	setData(parent, state, Role::CheckState);
}

bool FilteredProxyModel::Check(const QVariant & value, const std::function<bool(const QModelIndex &)> & f) const
{
	bool result = false;
	const auto indexList = value.value<QList<QModelIndex>>();
	EnumerateLeafs(*this, indexList.size() > 1 ? indexList : QList { QModelIndex {} }, [&] (const QModelIndex & child)
	{
		if (child.data(Role::Type).value<ItemType>() == ItemType::Books)
			result = f(child) || result;
	});
	return result;
}

QStringList FilteredProxyModel::CollectLanguages() const
{
	std::set<QString> languages;
	EnumerateLeafs(*this, {QModelIndex{}}, [&] (const QModelIndex & child)
	{
		if (child.data(Role::Type).value<ItemType>() == ItemType::Books)
			languages.emplace(child.data(Role::Lang).toString().toLower());
	});
	languages.erase(QString());

	return { languages.cbegin(), languages.cend() };
}

int FilteredProxyModel::GetCount() const
{
	int result = 0;
	EnumerateLeafs(*this, { QModelIndex{} }, [&] (const QModelIndex &)
	{
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
