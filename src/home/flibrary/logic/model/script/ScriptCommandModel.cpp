#include "ScriptCommandModel.h"

#include <plog/Log.h>

using namespace HomeCompa::Flibrary;

class ScriptCommandModel::Impl
{
public:
	explicit Impl(std::shared_ptr<IScriptController> scriptController)
		: m_scriptController(std::move(scriptController))
	{
	}

private:
	PropagateConstPtr<IScriptController, std::shared_ptr> m_scriptController;
};

ScriptCommandModel::ScriptCommandModel(std::shared_ptr<IScriptController> scriptController, QObject * parent)
	: QSortFilterProxyModel(parent)
	, m_impl(std::move(scriptController))
{
	PLOGD << "ScriptCommandModel created";
}

ScriptCommandModel::~ScriptCommandModel()
{
	PLOGD << "ScriptCommandModel destroyed";
}
