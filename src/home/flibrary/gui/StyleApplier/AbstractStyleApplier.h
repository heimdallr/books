#pragma once

#include "fnd/memory.h"

#include "interface/ui/IStyleApplier.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class AbstractStyleApplier : virtual public IStyleApplier
{
protected:
	AbstractStyleApplier(std::shared_ptr<ISettings> settings);

protected:
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
};

}
