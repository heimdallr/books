#include "ScriptModel.h"

#include "fnd/FindPair.h"
#include "fnd/ScopedCall.h"
#include "fnd/algorithm.h"

#include "interface/Localization.h"
#include "interface/logic/IScriptController.h"

#include "ScriptSortFilterModel.h"
#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

using Role = IScriptController::RoleScript;

}

ScriptModel::ScriptModel(const std::shared_ptr<IScriptControllerProvider>& scriptControllerProvider, QObject* parent)
	: QAbstractTableModel(parent)
	, m_scriptController(scriptControllerProvider->GetScriptController())
{
	PLOGV << "ScriptModel created";
}

ScriptModel::~ScriptModel()
{
	PLOGV << "ScriptModel destroyed";
}

int ScriptModel::columnCount(const QModelIndex& /*parent*/) const
{
	return 2;
}

int ScriptModel::rowCount(const QModelIndex& /*parent*/) const
{
	return static_cast<int>(m_scriptController->GetScripts().size());
}

QVariant ScriptModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
	constexpr const char* headers[] {
		QT_TRANSLATE_NOOP("ScriptModel", "Type"),
		QT_TRANSLATE_NOOP("ScriptModel", "Name"),
	};

	return orientation == Qt::Horizontal && role == Qt::DisplayRole ? Loc::Tr("ScriptModel", headers[section]) : QAbstractTableModel::headerData(section, orientation, role);
}

QVariant ScriptModel::data(const QModelIndex& index, const int role) const
{
	assert(index.isValid() && index.row() >= 0 && index.row() < rowCount());
	const auto& item = m_scriptController->GetScripts()[static_cast<size_t>(index.row())];

	switch (role)
	{
		case Qt::DisplayRole:
			switch (index.column())
			{
				case 0:
					return Loc::Tr(IScriptController::s_context, FindSecond(IScriptController::s_scriptTypes, item.type));

				case 1:
					return item.name;

				default:
					break;
			}
			break;

		case Role::Mode:
			return QVariant::fromValue(item.mode);

		case Role::Type:
			return static_cast<int>(item.type);

		case Role::Number:
			return item.number;

		case Role::Uid:
			return item.uid;

		default:
			break;
	}

	return {};
}

bool ScriptModel::setData(const QModelIndex& index, const QVariant& value, const int role)
{
	if (!index.isValid())
	{
		switch (role)
		{
			case Qt::EditRole:
				return m_scriptController->Save(), true;

			case Role::Observer:
				return Util::Set(m_observer, value.value<ISourceModelObserver*>());

			default:
				break;
		}

		return assert(false && "unexpected role"), false;
	}

	switch (role)
	{
		case Qt::EditRole:
			switch (index.column())
			{
				case 0:
					return m_scriptController->SetScriptType(index.row(), static_cast<IScriptController::Script::Type>(value.toInt()));

				case 1:
					return m_scriptController->SetScriptName(index.row(), value.toString());

				default:
					break;
			}
			break;

		case Role::Number:
			return m_scriptController->SetScriptNumber(index.row(), value.toInt());

		default:
			break;
	}

	return assert(false && "unexpected role"), false;
}

Qt::ItemFlags ScriptModel::flags(const QModelIndex& index) const
{
	return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
}

bool ScriptModel::insertRows(const int row, const int count, const QModelIndex& parent)
{
	const ScopedCall insertGuard(
		[&] {
			beginInsertRows(parent, row, row + count - 1);
		},
		[&] {
			endInsertRows();
		}
	);
	return m_scriptController->InsertScripts(row, count);
}

bool ScriptModel::removeRows(const int row, const int count, const QModelIndex& /*parent*/)
{
	const ScopedCall removeGuard([&] {
		if (m_observer)
			m_observer->OnRowsRemoved(row, count);
	});
	return m_scriptController->RemoveScripts(row, count);
}
