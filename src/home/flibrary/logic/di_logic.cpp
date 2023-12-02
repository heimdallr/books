#include "di_logic.h"

#include <Hypodermic/Hypodermic.h>

// ReSharper disable CppUnusedIncludeDirective
#include "Annotation/AnnotationController.h"
#include "Collection/CollectionController.h"
#include "data/DataProvider.h"
#include "data/ModelProvider.h"
#include "data/NavigationQueryExecutor.h"
#include "interface/ui/IUiFactory.h"
#include "log/LogController.h"
#include "logic/TreeViewController/AbstractTreeViewController.h"
#include "LogicFactory.h"
#include "model/FilteredProxyModel.h"
#include "model/ListModel.h"
#include "model/SortFilterProxyModel.h"
#include "model/TreeModel.h"
#include "shared/CommandLine.h"
#include "shared/DatabaseController.h"
#include "shared/DatabaseUser.h"
#include "shared/ProgressController.h"
#include "shared/TaskQueue.h"
#include "shared/ZipProgressCallback.h"
#include "userdata/UserDataController.h"
#include "util/ISettings.h"
// ReSharper restore CppUnusedIncludeDirective

namespace HomeCompa::Flibrary {

void DiLogic(Hypodermic::ContainerBuilder & builder, const std::shared_ptr<Hypodermic::Container> & container)
{
	builder.registerType<AnnotationController>().as<IAnnotationController>().singleInstance();
	builder.registerType<CollectionController>().as<ICollectionController>().singleInstance();
	builder.registerType<CommandLine>().as<ICommandLine>();
	builder.registerType<DatabaseController>().singleInstance();
	builder.registerType<DatabaseUser>().singleInstance();
	builder.registerType<DataProvider>().singleInstance();
	builder.registerType<FilteredProxyModel>().as<AbstractFilteredProxyModel>();
	builder.registerType<ListModel>().as<AbstractListModel>();
	builder.registerType<LogController>().as<ILogController>().singleInstance();
	builder.registerType<NavigationQueryExecutor>().as<INavigationQueryExecutor>().singleInstance();
	builder.registerType<ProgressController>().as<IProgressController>().singleInstance();
	builder.registerType<SortFilterProxyModel>().as<AbstractSortFilterProxyModel>();
	builder.registerType<TreeModel>().as<AbstractTreeModel>();
	builder.registerType<UserDataController>().as<IUserDataController>();

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
	}).as<IModelProvider>().singleInstance();

	builder.registerInstanceFactory([] (Hypodermic::ComponentContext & ctx)
	{
		return ctx.resolve<IUiFactory>()->GetTreeViewController();
	}).as<ITreeViewController>();

	builder.registerInstanceFactory([] (Hypodermic::ComponentContext & ctx)
	{
		return ctx.resolve<IUiFactory>()->GetTreeViewController();
	}).as<ITreeViewController>();

	builder.registerInstanceFactory([] (Hypodermic::ComponentContext &)
	{
		static auto taskQueue = std::make_shared<TaskQueue>();
		return taskQueue;
	}).as<ITaskQueue>();
}

}
