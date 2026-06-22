#include "FilteredProxyModel.h"

#include <set>

#include "interface/constants/Enums.h"
#include "interface/constants/ModelRole.h"

#include "ModelUtil.h"

using namespace HomeCompa::Flibrary;

namespace
{

Qt::CheckState GetCheckState(const QIdentityProxyModel& model, const QModelIndex& parent)
{
	assert(parent.isValid());

	if (parent.data(Role::Type).value<ItemType>() == ItemType::Books)
		return model.QIdentityProxyModel::data(parent, Qt::CheckStateRole).value<Qt::CheckState>();

	std::optional<Qt::CheckState> result;
	for (int i = 0, sz = model.rowCount(parent); i < sz; ++i)
	{
		const auto index      = model.index(i, 0, parent);
		const auto checkState = GetCheckState(model, index);
		if (checkState == Qt::PartiallyChecked)
			return Qt::PartiallyChecked;

		if (!result)
		{
			result = checkState;
			continue;
		}

		if (*result != checkState)
			return Qt::PartiallyChecked;
	}

	return result ? *result : Qt::Unchecked;
}

} // namespace

AbstractFilteredProxyModel::AbstractFilteredProxyModel(QObject* parent)
	: QIdentityProxyModel(parent)
{
}

FilteredProxyModel::FilteredProxyModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent)
	: AbstractFilteredProxyModel(parent)
	, m_sourceModel(modelProvider->GetSourceModel())
{
	QIdentityProxyModel::setSourceModel(m_sourceModel.get());
}

FilteredProxyModel::~FilteredProxyModel() = default;

QVariant FilteredProxyModel::data(const QModelIndex& index, const int role) const
{
	if (index.isValid())
	{
		if (role == Qt::CheckStateRole)
		{
			if (const auto value = index.data(Role::CheckableColumn); value.isValid())
				return index.column() == value.toInt() ? GetCheckState(*this, index) : QVariant {};
			return {};
		}

		return QIdentityProxyModel::data(index, role);
	}

	switch (role)
	{
		case Role::Count:
			return GetCount();

		default:
			break;
	}

	return QIdentityProxyModel::data(index, role);
}

bool FilteredProxyModel::setData(const QModelIndex& index, const QVariant& value, const int role)
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
			return Check(value, [&](const QModelIndex& bookIndex) {
				return setData(bookIndex, bookIndex.data(Qt::CheckStateRole).value<Qt::CheckState>() == Qt::Checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
			});

		default:
			break;
	}

	return QIdentityProxyModel::setData(index, value, role);
}

bool FilteredProxyModel::Check(const QVariant& value, const Qt::CheckState checkState)
{
	return Check(value, [&](const QModelIndex& index) {
		return index.data(Qt::CheckStateRole).value<Qt::CheckState>() != checkState && setData(index, checkState, Qt::CheckStateRole);
	});
}

void FilteredProxyModel::Check(const QModelIndex& parent, const Qt::CheckState state)
{
	const auto count = rowCount(parent);
	for (auto i = 0; i < count; ++i)
		Check(index(i, 0, parent), state);

	if (count > 0)
		emit dataChanged(index(0, 0, parent), index(count - 1, 0, parent), { Qt::CheckStateRole });

	setData(parent, state, Role::CheckState);
}

bool FilteredProxyModel::Check(const QVariant& value, const std::function<bool(const QModelIndex&)>& f) const
{
	bool       result    = false;
	const auto indexList = value.value<QList<QModelIndex>>();
	ModelUtil::EnumerateLeafs(*this, indexList.size() > 1 ? indexList : QList { QModelIndex {} }, [&](const QModelIndex& child) {
		if (child.data(Role::Type).value<ItemType>() == ItemType::Books)
			result = f(child) || result;
	});
	return result;
}

int FilteredProxyModel::GetCount() const
{
	std::unordered_set<QString> unique;
	ModelUtil::EnumerateLeafs(*this, { QModelIndex {} }, [&](const QModelIndex& index) {
		unique.emplace(index.data(Role::Id).toString());
	});

	return static_cast<int>(unique.size());
}

void FilteredProxyModel::GetSelected(const QModelIndex& index, const QModelIndexList& indexList, QModelIndexList* selected) const
{
	ModelUtil::EnumerateLeafs(*this, { QModelIndex {} }, [&](const QModelIndex& child) {
		if (child.data(Role::Type).value<ItemType>() == ItemType::Books && child.data(Role::CheckState).value<Qt::CheckState>() == Qt::Checked)
			(*selected) << child;
	});

	if (selected->isEmpty())
		ModelUtil::EnumerateLeafs(*this, indexList, [&](const QModelIndex& child) {
			if (child.data(Role::Type).value<ItemType>() == ItemType::Books)
				(*selected) << child;
		});

	if (selected->isEmpty())
		(*selected) << index;
}
