#include "di_logic.h"

#include <Hypodermic/Hypodermic.h>

#include "factory.h"

namespace HomeCompa::Flibrary {

void di_logic(Hypodermic::ContainerBuilder & builder, const std::shared_ptr<Hypodermic::Container> & /*container*/)
{
	builder.registerType<LogicFactory>().as<ILogicFactory>().singleInstance();
}

}
