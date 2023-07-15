#include "di_logic.h"

#include <Hypodermic/Hypodermic.h>

#include "factory.h"
#include "Collection/CollectionController.h"
#include "interface/logic/ITreeViewController.h"
#include "interface/ui/IUiFactory.h"
#include "util/ISettings.h"

namespace HomeCompa::Flibrary {

void di_logic(Hypodermic::ContainerBuilder & builder, const std::shared_ptr<Hypodermic::Container> & container)
{
	builder.registerType<CollectionController>().as<ICollectionController>();

	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<LogicFactory>(*container);
	}).as<ILogicFactory>().singleInstance();

	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<LogicFactory>(*container);
	}).as<ILogicFactory>().singleInstance();

	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext & ctx)
	{
		return ctx.resolve<IUiFactory>()->GetTreeViewController();
	}).as<ITreeViewController>();
}

}
