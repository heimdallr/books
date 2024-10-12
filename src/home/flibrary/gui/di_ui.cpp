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

#include "GuiUtil/interface/IParentWidgetProvider.h"
#include "util/Settings.h"

#include "delegate/OpenFileDialogDelegateEditor.h"
#include "delegate/StorableComboboxDelegateEditor.h"
#include "dialogs/AddCollectionDialog.h"
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
#include "ProgressBar.h"
#include "RateStarsProvider.h"
#include "UiFactory.h"
// ReSharper restore CppUnusedIncludeDirective

namespace HomeCompa::Flibrary {

void DiUi(Hypodermic::ContainerBuilder & builder, const std::shared_ptr<Hypodermic::Container> & container)
{
	builder.registerType<AddCollectionDialog>().as<IAddCollectionDialog>();
	builder.registerType<LineOption>().as<ILineOption>();
	builder.registerType<MainWindow>().as<IMainWindow>().singleInstance();
	builder.registerType<RateStarsProvider>().as<IRateStarsProvider>().singleInstance();
	builder.registerType<ScriptDialog>().as<IScriptDialog>();

	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<UiFactory>(*container);
	}).as<IUiFactory>().singleInstance();
}

}
