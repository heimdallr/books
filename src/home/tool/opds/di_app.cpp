#include "di_app.h"

#include <Hypodermic/Hypodermic.h>

#include "interface/logic/ICollectionProvider.h"

#include "logic/di_logic.h"
#include "requester/NoSqlRequester.h"
#include "requester/ReactAppRequester.h"
#include "requester/Requester.h"
#include "util/CoverCache.h"

#include "Server.h"

namespace HomeCompa::Opds
{

void DiInit(Hypodermic::ContainerBuilder& builder, std::shared_ptr<Hypodermic::Container>& container)
{
	Flibrary::DiLogic(builder, container);

	builder.registerType<CoverCache>().as<ICoverCache>().singleInstance();
	builder.registerType<NoSqlRequester>().as<INoSqlRequester>().singleInstance();
	builder.registerType<ReactAppRequester>().as<IReactAppRequester>().singleInstance();
	builder.registerType<Requester>().as<IRequester>().singleInstance();
	builder.registerType<Server>().as<IServer>().singleInstance();

	container = builder.build();
}

}
