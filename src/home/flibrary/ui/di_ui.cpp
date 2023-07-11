#include "di_ui.h"

#include <Hypodermic/Hypodermic.h>

#include "interface/constants/ProductConstant.h"
#include "interface/logic/ILogicFactory.h"

#include "util/Settings.h"

#include "MainWindow.h"
#include "UiFactory.h"

namespace HomeCompa::Flibrary {

void di_ui(Hypodermic::ContainerBuilder & builder, const std::shared_ptr<Hypodermic::Container> & container)
{
	builder.registerInstanceFactory([&] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<UiFactory>(*container);
	}
	).as<IUiFactory>().singleInstance();
	builder.registerType<MainWindow>().as<QMainWindow>().singleInstance();
	builder.registerInstanceFactory([] (Hypodermic::ComponentContext &)
	{
		return std::make_shared<Settings>(Constant::COMPANY_ID, Constant::PRODUCT_ID);
	}).as<ISettings>().singleInstance();
}

}
