#include "ScriptCommandModel.h"

#include <plog/Log.h>

#include "fnd/FindPair.h"
#include "fnd/ScopedCall.h"

#include "interface/constants/Localization.h"
#include "interface/logic/IScriptController.h"

#include "ScriptSortFilterModel.h"
#include "fnd/algorithm.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

using Role = IScriptController::RoleCommand;


class Model final
	: public QAbstractTableModel
{
public:
	static std::unique_ptr<QAbstractItemModel> Create(IScriptControllerProvider & scriptControllerProvider)
	{
		return std::make_unique<Model>(scriptControllerProvider.GetScriptController());
	}

public:
	explicit Model(std::shared_ptr<IScriptController> scriptController)
		: m_scriptController(std::move(scriptController))
	{
	}

private: // QAbstractItemModel
	int	columnCount(const QModelIndex & /*parent*/ = QModelIndex()) const override
	{
		return 3;
	}

	int	rowCount(const QModelIndex & /*parent*/ = QModelIndex()) const override
	{
		return static_cast<int>(m_scriptController->GetCommands().size());
	}

	QVariant headerData(const int section, const Qt::Orientation orientation, const int role) const override
	{
		constexpr const char * headers[]
		{
			QT_TRANSLATE_NOOP("ScriptCommandModel", "Type"),
			QT_TRANSLATE_NOOP("ScriptCommandModel", "Command"),
			QT_TRANSLATE_NOOP("ScriptCommandModel", "Arguments"),
		};

		return orientation == Qt::Horizontal && role == Qt::DisplayRole ? Loc::Tr("ScriptCommandModel", headers[section]) : QAbstractTableModel::headerData(section, orientation, role);
	}

	QVariant data(const QModelIndex & index, const int role) const override
	{
		if (!index.isValid())
			return role == Role::Uid ? m_uid : (assert(false && "unexpected role"), QVariant {});

		assert(index.row() >= 0 && index.row() < rowCount());
		const auto & item = m_scriptController->GetCommands()[index.row()];

		switch (role)
		{
			case Qt::DisplayRole:
				switch (index.column())
				{
					case 0:
						return Loc::Tr(IScriptController::s_context, FindSecond(IScriptController::s_commandTypes, item.type).type);

					case 1:
						return item.command;

					case 2:
						return item.args;

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
				return item.scriptUid;

			default:
				break;
		}

		return {};
	}

	bool setData(const QModelIndex & index, const QVariant & value, const int role) override
	{
		if (!index.isValid())
		{
			switch (role)
			{
				case Role::Observer:
					return Util::Set(m_observer, value.value<ISourceModelObserver *>(), *this);

				case Role::Uid:
					return Util::Set(m_uid, value.toString(), *this);

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
						return m_scriptController->SetCommandType(index.row(), static_cast<IScriptController::Command::Type>(value.toInt()));

					case 1:
						return m_scriptController->SetCommandCommand(index.row(), value.toString());

					case 2:
						return m_scriptController->SetCommandArgs(index.row(), value.toString());

					default:
						break;
				}
				break;

			case Role::Number:
				return m_scriptController->SetCommandNumber(index.row(), value.toInt());

			default:
				break;
		}

		return assert(false && "unexpected role"), false;
	}

	Qt::ItemFlags flags(const QModelIndex & index) const override
	{
		return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
	}

	bool insertRows(const int row, const int count, const QModelIndex & parent) override
	{
		const ScopedCall insertGuard([&] { beginInsertRows(parent, row, row + count - 1); }, [&] { endInsertRows(); });
		return m_scriptController->InsertCommand(m_uid, row, count);
	}

	bool removeRows(const int row, const int count, const QModelIndex & /*parent*/) override
	{
		const ScopedCall removeGuard([&]
		{
			if (m_observer)
				m_observer->OnRowsRemoved(row, count);
		});
		return m_scriptController->RemoveCommand(row, count);
	}

private:
	PropagateConstPtr<IScriptController, std::shared_ptr> m_scriptController;
	ISourceModelObserver * m_observer { nullptr };
	QString m_uid;
};

}

ScriptCommandModel::ScriptCommandModel(const std::shared_ptr<IScriptControllerProvider> & scriptControllerProvider, QObject * parent)
	: QSortFilterProxyModel(parent)
	, m_source(Model::Create(*scriptControllerProvider))
{
	QSortFilterProxyModel::setSourceModel(m_source.get());

	PLOGD << "ScriptCommandModel created";
}

ScriptCommandModel::~ScriptCommandModel()
{
	PLOGD << "ScriptCommandModel destroyed";
}

bool ScriptCommandModel::setData(const QModelIndex & index, const QVariant & value, const int role)
{
	const auto result = QSortFilterProxyModel::setData(index, value, role);
	if (result && role == Role::Uid)
		invalidateFilter();

	return result;
}

bool ScriptCommandModel::filterAcceptsRow(const int sourceRow, const QModelIndex & sourceParent) const
{
	return m_source->index(sourceRow, 0, sourceParent).data(Role::Uid) == m_source->data({}, Role::Uid);
}
