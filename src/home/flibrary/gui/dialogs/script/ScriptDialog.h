#pragma once

#include <QDialog>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IModelProvider.h"
#include "interface/ui/dialogs/IScriptDialog.h"

#include "gutil/interface/IParentWidgetProvider.h"
#include "util/ISettings.h"

#include "ComboBoxDelegate.h"
#include "CommandArgDelegate.h"
#include "CommandDelegate.h"
#include "ScriptNameDelegate.h"

namespace HomeCompa::Flibrary
{

class ScriptDialog final
	: public QDialog
	, virtual public IScriptDialog
{
	NON_COPY_MOVABLE(ScriptDialog)

public:
	ScriptDialog(
		const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider,
		const std::shared_ptr<const IModelProvider>&  modelProvider,
		std::shared_ptr<ISettings>                    settings,
		std::shared_ptr<ScriptComboBoxDelegate>       scriptTypeDelegate,
		std::shared_ptr<CommandComboBoxDelegate>      commandTypeDelegate,
		std::shared_ptr<ScriptNameDelegate>           scriptNameLineEditDelegate,
		std::shared_ptr<CommandDelegate>              commandDelegate,
		std::shared_ptr<CommandArgDelegate>           commandArgLineEditDelegate
	);
	~ScriptDialog() override;

private: // IScriptDialog
	int Exec() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
