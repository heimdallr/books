// ReSharper disable CppUnusedIncludeDirective
#include "di_ui.h"

#include <QCoreApplication>

#include <Hypodermic/Hypodermic.h>

#include "interface/constants/ProductConstant.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/ICollectionUpdateChecker.h"
#include "interface/logic/ICommandLine.h"
#include "interface/logic/IDatabaseChecker.h"
#include "interface/logic/ILogController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IModelProvider.h"
#include "interface/logic/IProgressController.h"
#include "interface/logic/IScriptController.h"
#include "interface/util/util.h"

#include "util/Settings.h"
#include "delegate/OpenFileDialogDelegateEditor.h"
#include "delegate/StorableComboboxDelegateEditor.h"
#include "dialogs/AddCollectionDialog.h"
#include "dialogs/Dialog.h"
#include "dialogs/script/ComboBoxDelegate.h"
#include "dialogs/script/CommandArgDelegate.h"
#include "dialogs/script/CommandDelegate.h"
#include "dialogs/script/ScriptDialog.h"
#include "dialogs/script/ScriptNameDelegate.h"

#include "AnnotationWidget.h"
#include "LineOption.h"
#include "LocaleController.h"
#include "LogItemDelegate.h"
#include "MainWindow.h"
#include "ParentWidgetProvider.h"
#include "ProgressBar.h"
#include "RateStarsProvider.h"
#include "UiFactory.h"

#include "config/version.h"
// ReSharper restore CppUnusedIncludeDirective

namespace HomeCompa::Flibrary {

void DiUi(Hypodermic::ContainerBuilder & builder, const std::shared_ptr<Hypodermic::Container> & container)
{
	builder.registerType<AboutDialog>().as<IAboutDialog>();
	builder.registerType<AddCollectionDialog>().as<IAddCollectionDialog>();
	builder.registerType<ErrorDialog>().as<IErrorDialog>();
	builder.registerType<InfoDialog>().as<IInfoDialog>();
	builder.registerType<InputTextDialog>().as<IInputTextDialog>();
	builder.registerType<LineOption>().as<ILineOption>();
	builder.registerType<MainWindow>().as<IMainWindow>().singleInstance();
	builder.registerType<ParentWidgetProvider>().singleInstance();
	builder.registerType<QuestionDialog>().as<IQuestionDialog>();
	builder.registerType<RateStarsProvider>().as<IRateStarsProvider>().singleInstance();
	builder.registerType<ScriptDialog>().as<IScriptDialog>();
	builder.registerType<WarningDialog>().as<IWarningDialog>();

	builder.registerInstanceFactory([] (Hypodermic::ComponentContext &)
	{
		return IsPortable()
			? std::make_shared<Settings>(QString("%1/%2.ini").arg(QCoreApplication::applicationDirPath()).arg(PRODUCT_ID))
			: std::make_shared<Settings>(COMPANY_ID, PRODUCT_ID);
	}).as<ISettings>().singleInstance();
	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<UiFactory>(*container);
	}).as<IUiFactory>().singleInstance();
}

}
