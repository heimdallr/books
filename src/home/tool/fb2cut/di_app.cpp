#include "di_app.h"

#include <Hypodermic/Hypodermic.h>

#include "GuiUtil/di_gui_util.h"
//#include "MainWindow.h"

namespace HomeCompa::fb2cut {

void DiInit(Hypodermic::ContainerBuilder & builder, std::shared_ptr<Hypodermic::Container> & container)
{
	Util::DiGuiUtil(builder, container);
//	builder.registerType<MainWindow>();

	container = builder.build();
}

}