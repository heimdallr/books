#include "di_logic.h"

#include <QCoreApplication>

#include <Hypodermic/Hypodermic.h>

// ReSharper disable CppUnusedIncludeDirective
#include "Annotation/AnnotationController.h"
#include "Collection/CollectionCleaner.h"
#include "Collection/CollectionController.h"
#include "Collection/CollectionProvider.h"
#include "Collection/CollectionUpdateChecker.h"
#include "data/DataProvider.h"
#include "data/ModelProvider.h"
#include "data/NavigationQueryExecutor.h"
#include "interface/ui/IUiFactory.h"
#include "log/LogController.h"
#include "logic/TreeViewController/AbstractTreeViewController.h"
#include "LogicFactory.h"
#include "database/DatabaseChecker.h"
#include "database/DatabaseController.h"
#include "database/DatabaseUser.h"
#include "model/FilteredProxyModel.h"
#include "model/ListModel.h"
#include "model/SortFilterProxyModel.h"
#include "model/TreeModel.h"
#include "script/CommaneExecutor.h"
#include "script/ScriptController.h"
#include "shared/CommandLine.h"
#include "shared/OpdsController.h"
#include "shared/ProgressController.h"
#include "shared/TaskQueue.h"
#include "shared/UpdateChecker.h"
#include "shared/ZipProgressCallback.h"
#include "shared/ReaderController.h"
#include "userdata/UserDataController.h"
#include "util/app.h"
#include "util/ISettings.h"
#include "util/Settings.h"
// ReSharper restore CppUnusedIncludeDirective

#include "config/version.h"

namespace HomeCompa::Flibrary {

void DiLogic(Hypodermic::ContainerBuilder & builder, const std::shared_ptr<Hypodermic::Container> & container)
{
	builder.registerType<AnnotationController>().as<IAnnotationController>().singleInstance();
	builder.registerType<CollectionCleaner>().as<ICollectionCleaner>();
	builder.registerType<CollectionController>().as<ICollectionController>().singleInstance();
	builder.registerType<CollectionProvider>().as<ICollectionProvider>().singleInstance();
	builder.registerType<CollectionUpdateChecker>().as<ICollectionUpdateChecker>();
	builder.registerType<CommandLine>().as<ICommandLine>();
	builder.registerType<DatabaseChecker>().as<IDatabaseChecker>();
	builder.registerType<DatabaseController>().as<IDatabaseController>().singleInstance();
	builder.registerType<DatabaseUser>().as<IDatabaseUser>().singleInstance();
	builder.registerType<DataProvider>().singleInstance();
	builder.registerType<FilteredProxyModel>().as<AbstractFilteredProxyModel>();
	builder.registerType<ListModel>().as<AbstractListModel>();
	builder.registerType<LogController>().as<ILogController>().singleInstance();
	builder.registerType<NavigationQueryExecutor>().as<INavigationQueryExecutor>().singleInstance();
	builder.registerType<OpdsController>().as<IOpdsController>();
	builder.registerType<ProgressController>().as<IAnnotationProgressController>();
	builder.registerType<ProgressController>().as<IBooksExtractorProgressController>().singleInstance();
	builder.registerType<ReaderController>().as<IReaderController>().singleInstance();
	builder.registerType<ScriptController>().as<IScriptController>();
	builder.registerType<SortFilterProxyModel>().as<AbstractSortFilterProxyModel>();
	builder.registerType<TreeModel>().as<AbstractTreeModel>();
	builder.registerType<UpdateChecker>().as<IUpdateChecker>();
	builder.registerType<UserDataController>().as<IUserDataController>();

	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<LogicFactory>(*container);
	}).as<ILogicFactory>().singleInstance();

	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<ModelProvider>(*container);
	}).as<IModelProvider>().singleInstance();

	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<ScriptControllerProvider>(*container);
	}).as<IScriptControllerProvider>().singleInstance();

	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<CommandExecutor>();
	}).as<IScriptController::ICommandExecutor>().singleInstance();

	builder.registerInstanceFactory([] (Hypodermic::ComponentContext &)
	{
		static auto taskQueue = std::make_shared<TaskQueue>();
		return taskQueue;
	}).as<ITaskQueue>();

	builder.registerInstanceFactory([] (Hypodermic::ComponentContext &)
	{
		return Util::GetInstallerDescription().type == Util::InstallerType::portable
			       ? std::make_shared<Settings>(QString("%1/%2.ini").arg(QCoreApplication::applicationDirPath()).arg(PRODUCT_ID))
			       : std::make_shared<Settings>(COMPANY_ID, PRODUCT_ID);
	}).as<ISettings>().singleInstance();
}

}
