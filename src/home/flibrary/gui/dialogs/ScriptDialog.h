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
	explicit ScriptDialog(const std::shared_ptr<class ParentWidgetProvider> & parentWidgetProvider
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<class IScriptController> scriptController
	);
	~ScriptDialog() override;

private: // IScriptDialog
	int Exec() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
