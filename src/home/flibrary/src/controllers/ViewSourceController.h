#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "models/SimpleModel.h"

class QVariant;

namespace HomeCompa {
class Settings;
}

namespace HomeCompa::Flibrary {

class ComboBoxController;

class ViewSourceController final
{
	NON_COPY_MOVABLE(ViewSourceController)

public:
	ViewSourceController(Settings & uiSetting, const char * keyName, const QVariant & defaultValue, SimpleModelItems simpleModelItems);
	~ViewSourceController();

	ComboBoxController * GetComboBoxController() noexcept;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
