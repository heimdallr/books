// ReSharper disable CppUnusedIncludeDirective
#include "di_gui_util.h"

#include <Hypodermic/Hypodermic.h>

#include <QCoreApplication>

#include "Dialog.h"
#include "UiFactory.h"
#include "ParentWidgetProvider.h"
#include "util/app.h"
#include "util/Settings.h"
#include "config/version.h"

// ReSharper restore CppUnusedIncludeDirective

namespace HomeCompa::Util {

void DiGuiUtil(Hypodermic::ContainerBuilder & builder, const std::shared_ptr<Hypodermic::Container> & container)
{
	builder.registerType<ParentWidgetProvider>().as<IParentWidgetProvider>().singleInstance();

	builder.registerType<ErrorDialog>().as<IErrorDialog>();
	builder.registerType<InfoDialog>().as<IInfoDialog>();
	builder.registerType<InputTextDialog>().as<IInputTextDialog>();
	builder.registerType<QuestionDialog>().as<IQuestionDialog>();
	builder.registerType<WarningDialog>().as<IWarningDialog>();

	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<UiFactory>(*container);
	}).as<IUiFactory>().singleInstance();

	builder.registerInstanceFactory([] (Hypodermic::ComponentContext &)
	{
		return GetInstallerDescription().type == InstallerType::portable
			? std::make_shared<Settings>(QString("%1/%2.ini").arg(QCoreApplication::applicationDirPath()).arg(PRODUCT_ID))
			: std::make_shared<Settings>(COMPANY_ID, PRODUCT_ID);
	}).as<ISettings>().singleInstance();
}

}
