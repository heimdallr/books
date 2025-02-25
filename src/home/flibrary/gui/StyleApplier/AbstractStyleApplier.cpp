#include "AbstractStyleApplier.h"

using namespace HomeCompa::Flibrary;

AbstractStyleApplier::AbstractStyleApplier(std::shared_ptr<ISettings> settings)
	: m_settings { std::move(settings) }
{
}
