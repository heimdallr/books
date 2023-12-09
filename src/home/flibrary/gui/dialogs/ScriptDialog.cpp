#include "ui_ScriptDialog.h"
#include "ScriptDialog.h"

#include <plog/Log.h>

#include "interface/logic/IModelProvider.h"

#include "GeometryRestorable.h"
#include "ParentWidgetProvider.h"

using namespace HomeCompa::Flibrary;

class ScriptDialog::Impl final
	: GeometryRestorable
	, GeometryRestorableObserver
{
public:
	Impl(ScriptDialog& self
		, const IModelProvider & modelProvider
		, std::shared_ptr<ISettings> settings
	)
		: GeometryRestorable(*this, std::move(settings), "ScriptDialog")
		, GeometryRestorableObserver(self)
		, m_self(self)
		, m_scriptModel(modelProvider.CreateScriptModel())
		, m_scriptCommandModel(modelProvider.CreateScriptCommandModel())
	{
		m_ui.setupUi(&m_self);
		m_ui.viewScript->setModel(m_scriptModel.get());
		m_ui.viewCommand->setModel(m_scriptCommandModel.get());
		Init();
	}

private:
	ScriptDialog & m_self;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_scriptModel;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_scriptCommandModel;
	Ui::ScriptDialog m_ui{};
};

ScriptDialog::ScriptDialog(const std::shared_ptr<ParentWidgetProvider> & parentWidgetProvider
	, const std::shared_ptr<const IModelProvider> & modelProvider
	, std::shared_ptr<ISettings> settings
)
	: QDialog(parentWidgetProvider->GetWidget())
	, m_impl(*this
		, *modelProvider
		, std::move(settings)
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
