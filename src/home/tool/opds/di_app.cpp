#include "di_app.h"

#include <Hypodermic/Hypodermic.h>

#include "logic/di_logic.h"
#include "util/ISettings.h"

#include "Server.h"

namespace HomeCompa::Opds {

void DiInit(Hypodermic::ContainerBuilder & builder, std::shared_ptr<Hypodermic::Container> & container)
{
	Flibrary::DiLogic(builder, container);

	builder.registerType<Server>().as<IServer>().singleInstance();

	container = builder.build();
}

}
