#include "di_logic.h"

#include <QCoreApplication>

#include <Hypodermic/Hypodermic.h>

#include "interface/constants/SettingsConstant.h"

#include "Annotation/AnnotationController.h"
#include "Annotation/AuthorAnnotationController.h"
#include "ChangeNavigationController/SearchController.h"
#include "Collection/CollectionCleaner.h"
#include "Collection/CollectionController.h"
#include "Collection/CollectionProvider.h"
#include "Collection/CollectionUpdateChecker.h"
#include "JokeRequester/factory/JokeRequesterFactory.h"
#include "data/DataProvider.h"
#include "data/GenreFilterProvider.h"
#include "data/ModelProvider.h"
#include "data/NavigationQueryExecutor.h"
#include "database/DatabaseChecker.h"
#include "database/DatabaseController.h"
#include "database/DatabaseMigrator.h"
#include "database/DatabaseUser.h"
#include "log/LogController.h"
#include "model/FilteredProxyModel.h"
#include "model/GenreModel.h"
#include "model/LanguageModel.h"
#include "model/ListModel.h"
#include "model/SortFilterProxyModel.h"
#include "model/TreeModel.h"
#include "script/CommandExecutor.h"
#include "script/ScriptController.h"
#include "shared/CommandLine.h"
#include "shared/LibRateProvider.h"
#include "shared/OpdsController.h"
#include "shared/ProgressController.h"
#include "shared/ReaderController.h"
#include "shared/TaskQueue.h"
#include "shared/UpdateChecker.h"
#include "userdata/UserDataController.h"
#include "util/ISettings.h"
#include "util/Settings.h"
#include "util/app.h"

#include "LogicFactory.h"

#include "config/version.h"

namespace HomeCompa::Flibrary
{

void DiLogic(Hypodermic::ContainerBuilder& builder, const std::shared_ptr<Hypodermic::Container>& container)
{
	builder.registerType<CollectionCleaner>().as<ICollectionCleaner>();
	builder.registerType<CollectionUpdateChecker>().as<ICollectionUpdateChecker>();
	builder.registerType<CommandLine>().as<ICommandLine>();
	builder.registerType<DatabaseChecker>().as<IDatabaseChecker>();
	builder.registerType<DatabaseMigrator>().as<IDatabaseMigrator>();
	builder.registerType<FilteredProxyModel>().as<AbstractFilteredProxyModel>();
	builder.registerType<GenreModel>().as<IGenreModel>();
	builder.registerType<LanguageModel>().as<ILanguageModel>();
	builder.registerType<ListModel>().as<AbstractListModel>();
	builder.registerType<OpdsController>().as<IOpdsController>();
	builder.registerType<ProgressController>().as<IAnnotationProgressController>();
	builder.registerType<ScriptController>().as<IScriptController>();
	builder.registerType<TreeModel>().as<AbstractTreeModel>();
	builder.registerType<UpdateChecker>().as<IUpdateChecker>();
	builder.registerType<UserDataController>().as<IUserDataController>();
	builder.registerType<SortFilterProxyModel>().as<AbstractSortFilterProxyModel>();

	builder.registerType<AnnotationController>().as<IAnnotationController>().singleInstance();
	builder.registerType<AuthorAnnotationController>().as<IAuthorAnnotationController>().singleInstance();
	builder.registerType<CollectionController>().as<ICollectionController>().singleInstance();
	builder.registerType<CollectionProvider>().as<ICollectionProvider>().singleInstance();
	builder.registerType<CommandExecutor>().as<IScriptController::ICommandExecutor>().singleInstance();
	builder.registerType<DatabaseController>().as<IDatabaseController>().singleInstance();
	builder.registerType<DatabaseUser>().as<IDatabaseUser>().singleInstance();
	builder.registerType<DataProvider>().as<IDataProvider>().singleInstance();
	builder.registerType<GenreFilterProvider>().as<IGenreFilterController>().singleInstance();
	builder.registerType<LogController>().as<ILogController>().singleInstance();
	builder.registerType<NavigationQueryExecutor>().as<INavigationQueryExecutor>().singleInstance();
	builder.registerType<ProgressController>().as<IBooksExtractorProgressController>().singleInstance();
	builder.registerType<ReaderController>().as<IReaderController>().singleInstance();
	builder.registerType<SearchController>().as<IBookSearchController>().singleInstance();

	builder.registerInstanceFactory([&](Hypodermic::ComponentContext& ctx) { return ctx.resolve<IDataProvider>(); }).as<IBookInfoProvider>();
	builder.registerInstanceFactory([&](Hypodermic::ComponentContext& ctx) { return ctx.resolve<IGenreFilterController>(); }).as<IGenreFilterProvider>();
	builder
		.registerInstanceFactory(
			[&](Hypodermic::ComponentContext& ctx)
			{
				const auto settings = ctx.resolve<ISettings>();
				return settings->Get(Constant::Settings::LIBRATE_VIEW_PRECISION_KEY, Constant::Settings::LIBRATE_VIEW_PRECISION_DEFAULT) <= Constant::Settings::LIBRATE_VIEW_PRECISION_DEFAULT
		                 ? std::shared_ptr<AbstractLibRateProvider> { ctx.resolve<LibRateProviderSimple>() }
		                 : std::shared_ptr<AbstractLibRateProvider> { ctx.resolve<LibRateProviderDouble>() };
			})
		.as<ILibRateProvider>()
		.singleInstance();
	builder.registerInstanceFactory([&](Hypodermic::ComponentContext& ctx) { return ctx.resolve<IDataProvider>(); }).as<INavigationInfoProvider>();

	builder.registerInstanceFactory([&](Hypodermic::ComponentContext&) { return std::make_shared<JokeRequesterFactory>(*container); }).as<IJokeRequesterFactory>().singleInstance();

	builder.registerInstanceFactory([&](Hypodermic::ComponentContext&) { return std::make_shared<LogicFactory>(*container); }).as<ILogicFactory>().singleInstance();

	builder.registerInstanceFactory([&](Hypodermic::ComponentContext&) { return std::make_shared<ModelProvider>(*container); }).as<IModelProvider>().singleInstance();

	builder.registerInstanceFactory([&](Hypodermic::ComponentContext&) { return std::make_shared<ScriptControllerProvider>(*container); }).as<IScriptControllerProvider>().singleInstance();

	builder
		.registerInstanceFactory(
			[](Hypodermic::ComponentContext&)
			{
				static auto taskQueue = std::make_shared<TaskQueue>();
				return taskQueue;
			})
		.as<ITaskQueue>();

	builder
		.registerInstanceFactory(
			[](Hypodermic::ComponentContext&)
			{
				return Util::GetInstallerDescription().type == Util::InstallerType::portable ? std::make_shared<Settings>(QString("%1/%2.ini").arg(QCoreApplication::applicationDirPath()).arg(PRODUCT_ID))
		                                                                                     : std::make_shared<Settings>(COMPANY_ID, PRODUCT_ID);
			})
		.as<ISettings>()
		.singleInstance();
}

} // namespace HomeCompa::Flibrary
