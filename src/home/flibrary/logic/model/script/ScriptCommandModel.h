#pragma once

#include <QSortFilterProxyModel>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa::Flibrary {

class ScriptCommandModel final
	: public QSortFilterProxyModel
{
	NON_COPY_MOVABLE(ScriptCommandModel)

public:
	explicit ScriptCommandModel(std::shared_ptr<class IScriptController> scriptController, QObject * parent = nullptr);
	~ScriptCommandModel() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
