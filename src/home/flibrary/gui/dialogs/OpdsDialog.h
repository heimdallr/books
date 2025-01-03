#pragma once

#include <QDialog>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa {
class IParentWidgetProvider;
class ISettings;
}

namespace HomeCompa::Flibrary {

class OpdsDialog : public QDialog
{
	NON_COPY_MOVABLE(OpdsDialog)

public:
	OpdsDialog(const std::shared_ptr<IParentWidgetProvider> & parentWidgetProvider
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<class IOpdsController> opdsController
		, QWidget * parent = nullptr);
	~OpdsDialog() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
