#pragma once

#include <QDialog>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/ui/dialogs/IScriptDialog.h"

namespace HomeCompa
{
class IParentWidgetProvider;
class ISettings;
}

namespace HomeCompa::Flibrary
{

class ScriptDialog final
	: public QDialog
	, virtual public IScriptDialog
{
	NON_COPY_MOVABLE(ScriptDialog)

public:
	ScriptDialog(const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider,
	             const std::shared_ptr<const class IModelProvider>& modelProvider,
	             std::shared_ptr<ISettings> settings,
	             std::shared_ptr<class ScriptComboBoxDelegate> scriptTypeDelegate,
	             std::shared_ptr<class CommandComboBoxDelegate> commandTypeDelegate,
	             std::shared_ptr<class ScriptNameDelegate> scriptNameLineEditDelegate,
	             std::shared_ptr<class CommandDelegate> commandDelegate,
	             std::shared_ptr<class CommandArgDelegate> commandArgLineEditDelegate);
	~ScriptDialog() override;

private: // IScriptDialog
	int Exec() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
