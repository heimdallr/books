#pragma once

#include "fnd/ISettings.h"
#include "fnd/memory.h"

#include "interface/ui/IStyleApplier.h"

namespace HomeCompa::Flibrary {

class AbstractStyleApplier : public IStyleApplier
{
protected:
	explicit AbstractStyleApplier(std::shared_ptr<ISettings> settings);

protected:
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
};

} // namespace HomeCompa::Flibrary
