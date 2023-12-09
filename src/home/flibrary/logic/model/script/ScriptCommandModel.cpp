#include "ScriptCommandModel.h"

#include <plog/Log.h>

#include "interface/logic/IScriptController.h"

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

ScriptCommandModel::ScriptCommandModel(const std::shared_ptr<IScriptControllerProvider> & scriptControllerProvider, QObject * parent)
	: QSortFilterProxyModel(parent)
	, m_impl(scriptControllerProvider->GetScriptController())
{
	PLOGD << "ScriptCommandModel created";
}

ScriptCommandModel::~ScriptCommandModel()
{
	PLOGD << "ScriptCommandModel destroyed";
}
