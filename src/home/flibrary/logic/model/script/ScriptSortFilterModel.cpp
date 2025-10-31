#include "ScriptSortFilterModel.h"

#include "interface/logic/IModelProvider.h"
#include "interface/logic/IScriptController.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

using Role = IScriptController::RoleBase;

bool MoveRow(QSortFilterProxyModel& model, const QModelIndex& index, const int offset)
{
	const auto from    = index.data(Role::Number).toInt();
	const auto indexTo = model.index(index.row() + offset, index.column(), index.parent());
	const auto to      = indexTo.data(Role::Number).toInt();

	const auto result = model.setData(index, to, Role::Number) && model.setData(indexTo, from, Role::Number);

	if (result)
		model.invalidate();

	return result;
}

}

ScriptSortFilterModel::ScriptSortFilterModel(const std::shared_ptr<const IModelProvider>& modelProvider, QObject* parent)
	: QSortFilterProxyModel(parent)
	, m_source(modelProvider->GetSourceModel())
{
	QSortFilterProxyModel::setSourceModel(m_source.get());
	QSortFilterProxyModel::sort(0);

	m_source->setData({}, QVariant::fromValue<ISourceModelObserver*>(this), Role::Observer);

	PLOGV << "ScriptSortFilterModel created";
}

ScriptSortFilterModel::~ScriptSortFilterModel()
{
	PLOGV << "ScriptSortFilterModel destroyed";
}

bool ScriptSortFilterModel::setData(const QModelIndex& index, const QVariant& value, const int role)
{
	switch (role)
	{
		case Role::Up:
			assert(index.row() > 0);
			return MoveRow(*this, index, -1);

		case Role::Down:
			assert(index.isValid() && index.row() < rowCount() - 1);
			return MoveRow(*this, index, 1);

		default:
			break;
	}

	return QSortFilterProxyModel::setData(index, value, role);
}

bool ScriptSortFilterModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
	return m_source->index(sourceRow, 0, sourceParent).data(Role::Mode).value<IScriptController::Mode>() != IScriptController::Mode::Removed;
}

bool ScriptSortFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
	return left.data(Role::Number).toInt() < right.data(Role::Number).toInt();
}

void ScriptSortFilterModel::OnRowsRemoved(int /*row*/, int /*count*/)
{
	beginFilterChange();
	endFilterChange(Direction::Rows);
}
