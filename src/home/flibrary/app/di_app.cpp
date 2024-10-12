#include "di_app.h"

#include <Hypodermic/Hypodermic.h>

#include "logic/di_logic.h"
#include "gui/di_ui.h"
#include "GuiUtil/di_gui_util.h"

namespace HomeCompa::Flibrary {

void DiInit(Hypodermic::ContainerBuilder & builder, std::shared_ptr<Hypodermic::Container> & container)
{
	DiLogic(builder, container);
	DiUi(builder, container);
	Util::DiGuiUtil(builder, container);

	container = builder.build();
}

}
