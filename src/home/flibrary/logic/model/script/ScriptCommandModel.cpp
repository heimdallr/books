#include "ScriptCommandModel.h"

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

using Role = IScriptController::RoleCommand;

class Model final : public QAbstractTableModel
{
public:
	static std::unique_ptr<QAbstractItemModel> Create(IScriptControllerProvider& scriptControllerProvider)
	{
		return std::make_unique<Model>(scriptControllerProvider.GetScriptController());
	}

public:
	explicit Model(std::shared_ptr<IScriptController> scriptController)
		: m_scriptController(std::move(scriptController))
	{
	}

private: // QAbstractItemModel
	int columnCount(const QModelIndex& /*parent*/ = QModelIndex()) const override
	{
		return 1;
	}

	int rowCount(const QModelIndex& /*parent*/ = QModelIndex()) const override
	{
		return static_cast<int>(m_scriptController->GetCommands().size());
	}

	QVariant headerData(const int section, const Qt::Orientation orientation, const int role) const override
	{
		static constexpr const char* header = QT_TRANSLATE_NOOP("ScriptCommandModel", "Command");
		return orientation == Qt::Horizontal && role == Qt::DisplayRole ? QVariant::fromValue(Loc::Tr("ScriptCommandModel", header)) : QAbstractTableModel::headerData(section, orientation, role);
	}

	QVariant data(const QModelIndex& index, const int role) const override
	{
		if (!index.isValid())
			return role == Role::Uid ? m_uid : (assert(false && "unexpected role"), QVariant {});

		assert(index.row() >= 0 && index.row() < rowCount());
		const auto& item = m_scriptController->GetCommands()[static_cast<size_t>(index.row())];

		switch (role)
		{
			case Qt::DisplayRole:
				return QString("%1 %2").arg(Loc::Tr("ScriptController", item.command.toStdString().data()), item.args);

			case Role::Name:
				return item.command;

			case Role::Mode:
				return QVariant::fromValue(item.mode);

			case Role::Type:
				return QVariant::fromValue(item.type);

			case Role::Command:
				return item.command;

			case Role::Arguments:
				return item.args;

			case Role::WorkingFolder:
				return item.workingFolder;

			case Role::Number:
				return item.number;

			case Role::Uid:
				return item.scriptUid;

			default:
				break;
		}

		return {};
	}

	bool setData(const QModelIndex& index, const QVariant& value, const int role) override
	{
		if (!index.isValid())
		{
			switch (role)
			{
				case Role::Observer:
					return Util::Set(m_observer, value.value<ISourceModelObserver*>());

				case Role::Uid:
					return Util::Set(m_uid, value.toString());

				default:
					break;
			}

			return assert(false && "unexpected role"), false;
		}

		switch (role)
		{
			case Role::Type:
				return m_scriptController->SetCommandType(index.row(), value.value<IScriptController::Command::Type>());

			case Role::Command:
				return m_scriptController->SetCommandCommand(index.row(), value.toString()) && (emit dataChanged(index, index, { Qt::DisplayRole }), true);

			case Role::Arguments:
				return m_scriptController->SetCommandArgs(index.row(), value.toString()) && (emit dataChanged(index, index, { Qt::DisplayRole }), true);

			case Role::WorkingFolder:
				return m_scriptController->SetCommandWorkingFolder(index.row(), value.toString());

			case Role::Number:
				return m_scriptController->SetCommandNumber(index.row(), value.toInt());

			default:
				break;
		}

		return assert(false && "unexpected role"), false;
	}

	bool insertRows(const int row, const int count, const QModelIndex& parent) override
	{
		const ScopedCall insertGuard(
			[&] {
				beginInsertRows(parent, row, row + count - 1);
			},
			[&] {
				endInsertRows();
			}
		);
		return m_scriptController->InsertCommand(m_uid, row, count);
	}

	bool removeRows(const int row, const int count, const QModelIndex& /*parent*/) override
	{
		const ScopedCall removeGuard([&] {
			if (m_observer)
				m_observer->OnRowsRemoved(row, count);
		});
		return m_scriptController->RemoveCommand(row, count);
	}

private:
	PropagateConstPtr<IScriptController, std::shared_ptr> m_scriptController;
	ISourceModelObserver*                                 m_observer { nullptr };
	QString                                               m_uid;
};

} // namespace

ScriptCommandModel::ScriptCommandModel(const std::shared_ptr<IScriptControllerProvider>& scriptControllerProvider, QObject* parent)
	: QSortFilterProxyModel(parent)
	, m_source(Model::Create(*scriptControllerProvider))
{
	QSortFilterProxyModel::setSourceModel(m_source.get());

	PLOGV << "ScriptCommandModel created";
}

ScriptCommandModel::~ScriptCommandModel()
{
	PLOGV << "ScriptCommandModel destroyed";
}

bool ScriptCommandModel::setData(const QModelIndex& index, const QVariant& value, const int role)
{
	const auto result = QSortFilterProxyModel::setData(index, value, role);
	if (result && role == Role::Uid)
	{
		beginFilterChange();
		endFilterChange(Direction::Rows);
	}

	return result;
}

bool ScriptCommandModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
	return m_source->index(sourceRow, 0, sourceParent).data(Role::Uid) == m_source->data({}, Role::Uid);
}
