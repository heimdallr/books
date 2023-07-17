#include "di_logic.h"

#include <Hypodermic/Hypodermic.h>

// ReSharper disable CppUnusedIncludeDirective
#include "Collection/CollectionController.h"
#include "data/DataProvider.h"
#include "data/ModelProvider.h"
#include "interface/ui/IUiFactory.h"
#include "logic/TreeViewController/AbstractTreeViewController.h"
#include "LogicFactory.h"
#include "model/ListModel.h"
#include "model/TreeModel.h"
#include "util/ISettings.h"
// ReSharper restore CppUnusedIncludeDirective

namespace HomeCompa::Flibrary {

void DiLogic(Hypodermic::ContainerBuilder & builder, const std::shared_ptr<Hypodermic::Container> & container)
{
	builder.registerType<CollectionController>().as<ICollectionController>().singleInstance();
	builder.registerType<DataProvider>().singleInstance();
	builder.registerType<ListModel>().as<AbstractListModel>();
	builder.registerType<TreeModel>().as<AbstractTreeModel>();

	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<LogicFactory>(*container);
	}).as<ILogicFactory>().singleInstance();

	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<LogicFactory>(*container);
	}).as<ILogicFactory>().singleInstance();

	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<ModelProvider>(*container);
	}).as<AbstractModelProvider>().singleInstance();

	builder.registerInstanceFactory([] (Hypodermic::ComponentContext & ctx)
	{
		return ctx.resolve<IUiFactory>()->GetTreeViewController();
	}).as<ITreeViewController>();

	builder.registerInstanceFactory([] (Hypodermic::ComponentContext & ctx)
	{
		return ctx.resolve<IUiFactory>()->GetTreeViewController();
	}).as<ITreeViewController>();
}

}
