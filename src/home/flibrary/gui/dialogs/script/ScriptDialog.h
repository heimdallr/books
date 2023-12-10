#pragma once

#include <QDialog>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/ui/dialogs/IScriptDialog.h"

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class ScriptDialog final
	: public QDialog
	, virtual public IScriptDialog
{
	NON_COPY_MOVABLE(ScriptDialog)

public:
	ScriptDialog(const std::shared_ptr<class ParentWidgetProvider> & parentWidgetProvider
		, const std::shared_ptr<const class IModelProvider> & modelProvider
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<class ScriptComboBoxDelegate> scriptTypeDelegate
		, std::shared_ptr<class CommandComboBoxDelegate> commandTypeDelegate
		, std::shared_ptr<class ScriptNameLineEditDelegate> scriptNameLineEditDelegate
		, std::shared_ptr<class CommandArgLineEditDelegate> commandArgLineEditDelegate
	);
	~ScriptDialog() override;

private: // IScriptDialog
	int Exec() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
