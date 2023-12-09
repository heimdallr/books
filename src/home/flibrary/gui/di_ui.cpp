// ReSharper disable CppUnusedIncludeDirective
#include "di_ui.h"

#include <Hypodermic/Hypodermic.h>

#include "interface/constants/ProductConstant.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/ICommandLine.h"
#include "interface/logic/ILogController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IModelProvider.h"
#include "interface/logic/IProgressController.h"
#include "interface/logic/IScriptController.h"

#include "util/Settings.h"
#include "dialogs/AddCollectionDialog.h"
#include "dialogs/Dialog.h"
#include "dialogs/script/ComboBoxDelegate.h"
#include "dialogs/script/ScriptDialog.h"

#include "AnnotationWidget.h"
#include "LogItemDelegate.h"
#include "LocaleController.h"
#include "MainWindow.h"
#include "ParentWidgetProvider.h"
#include "ProgressBar.h"
#include "UiFactory.h"

#include "config/version.h"
// ReSharper restore CppUnusedIncludeDirective

namespace HomeCompa::Flibrary {

void DiUi(Hypodermic::ContainerBuilder & builder, const std::shared_ptr<Hypodermic::Container> & container)
{
	builder.registerType<AddCollectionDialog>().as<IAddCollectionDialog>();
	builder.registerType<ParentWidgetProvider>().singleInstance();
	builder.registerType<AboutDialog>().as<IAboutDialog>();
	builder.registerType<ErrorDialog>().as<IErrorDialog>();
	builder.registerType<InfoDialog>().as<IInfoDialog>();
	builder.registerType<InputTextDialog>().as<IInputTextDialog>();
	builder.registerType<QuestionDialog>().as<IQuestionDialog>();
	builder.registerType<ScriptDialog>().as<IScriptDialog>();
	builder.registerType<WarningDialog>().as<IWarningDialog>();
	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<ComboBoxDelegate>();
	}).as<ScriptComboBoxDelegate>();
	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<ComboBoxDelegate>();
	}).as<CommandComboBoxDelegate>();
	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<UiFactory>(*container);
	}).as<IUiFactory>().singleInstance();
	builder.registerType<MainWindow>().as<QMainWindow>().singleInstance();
	builder.registerInstanceFactory([] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<Settings>(COMPANY_ID, PRODUCT_ID);
	}).as<ISettings>().singleInstance();
}

}
