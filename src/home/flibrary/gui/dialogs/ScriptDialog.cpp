#include "ui_ScriptDialog.h"
#include "ScriptDialog.h"

#include <plog/Log.h>

#include "GeometryRestorable.h"
#include "ParentWidgetProvider.h"

using namespace HomeCompa::Flibrary;

class ScriptDialog::Impl final
	: GeometryRestorable
	, GeometryRestorableObserver
{
public:
	Impl(ScriptDialog& self
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<IScriptController> scriptController
	)
		: GeometryRestorable(*this, std::move(settings), "ScriptDialog")
		, GeometryRestorableObserver(self)
		, m_self(self)
		, m_scriptController(std::move(scriptController))
	{
		m_ui.setupUi(&m_self);
		Init();
	}

private:
	ScriptDialog & m_self;
	PropagateConstPtr<IScriptController, std::shared_ptr> m_scriptController;
	Ui::ScriptDialog m_ui{};
};

ScriptDialog::ScriptDialog(const std::shared_ptr<ParentWidgetProvider> & parentWidgetProvider
	, std::shared_ptr<ISettings> settings
	, std::shared_ptr<IScriptController> scriptController
)
	: QDialog(parentWidgetProvider->GetWidget())
	, m_impl(*this
		, std::move(settings)
		, std::move(scriptController)
	)
{
	PLOGD << "ScriptDialog created";
}

ScriptDialog::~ScriptDialog()
{
	PLOGD << "ScriptDialog destroyed";
}

int ScriptDialog::Exec()
{
	return QDialog::exec();
}
