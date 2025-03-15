#include "di_gui_util.h"

#include <Hypodermic/Hypodermic.h>

#include "Dialog.h"
#include "ParentWidgetProvider.h"
#include "UiFactory.h"

namespace HomeCompa::Util
{

void DiGuiUtil(Hypodermic::ContainerBuilder& builder, const std::shared_ptr<Hypodermic::Container>& container)
{
	builder.registerType<ParentWidgetProvider>().as<IParentWidgetProvider>().singleInstance();

	builder.registerType<ErrorDialog>().as<IErrorDialog>();
	builder.registerType<InfoDialog>().as<IInfoDialog>();
	builder.registerType<InputTextDialog>().as<IInputTextDialog>();
	builder.registerType<QuestionDialog>().as<IQuestionDialog>();
	builder.registerType<WarningDialog>().as<IWarningDialog>();

	builder.registerInstanceFactory([&](Hypodermic::ComponentContext&) { return std::make_shared<UiFactory>(*container); }).as<IUiFactory>().singleInstance();
}

}
