#include "ScriptModel.h"

#include <plog/Log.h>

using namespace HomeCompa::Flibrary;

class ScriptModel::Impl
{
public:
	explicit Impl(std::shared_ptr<IScriptController> scriptController)
		: m_scriptController(std::move(scriptController))
	{
	}

private:
	PropagateConstPtr<IScriptController, std::shared_ptr> m_scriptController;
};

ScriptModel::ScriptModel(std::shared_ptr<IScriptController> scriptController, QObject * parent)
	: QAbstractTableModel(parent)
	, m_impl(std::move(scriptController))
{
	PLOGD << "ScriptModel created";
}

ScriptModel::~ScriptModel()
{
	PLOGD << "ScriptModel destroyed";
}

int	ScriptModel::columnCount(const QModelIndex & /*parent*/) const
{
	return 2;
}

int	ScriptModel::rowCount(const QModelIndex & /*parent*/) const
{
	return {};
}

QVariant ScriptModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
	constexpr const char* headers[]
	{
		QT_TR_NOOP("Type"),
		QT_TR_NOOP("Name"),
	};

	return orientation == Qt::Horizontal && role == Qt::DisplayRole ? tr(headers[section]) : QAbstractTableModel::headerData(section, orientation, role);
}

QVariant ScriptModel::data(const QModelIndex & /*index*/, const int /*role*/) const
{
	return {};
}

bool ScriptModel::setData(const QModelIndex & /*index*/, const QVariant & /*value*/, const int /*role*/)
{
	return {};
}

Qt::ItemFlags ScriptModel::flags(const QModelIndex & /*index*/) const
{
	return {};
}
