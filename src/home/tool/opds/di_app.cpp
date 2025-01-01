#include "di_app.h"

#include <Hypodermic/Hypodermic.h>

#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseController.h"

#include "logic/di_logic.h"
#include "util/ISettings.h"

#include "Server.h"
#include "Requester.h"

namespace HomeCompa::Opds {

void DiInit(Hypodermic::ContainerBuilder & builder, std::shared_ptr<Hypodermic::Container> & container)
{
	Flibrary::DiLogic(builder, container);

	builder.registerType<Requester>().as<IRequester>().singleInstance();
	builder.registerType<Server>().as<IServer>().singleInstance();

	container = builder.build();
}

}
