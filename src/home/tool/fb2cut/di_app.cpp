#include "di_app.h"

#include "GuiUtil/di_gui_util.h"
#include "Hypodermic/Hypodermic.h"
#include "logic/di_logic.h"

namespace HomeCompa::fb2cut
{

void DiInit(Hypodermic::ContainerBuilder& builder, std::shared_ptr<Hypodermic::Container>& container)
{
	Util::DiGuiUtil(builder, container);
	Flibrary::DiLogic(builder, container);

	container = builder.build();
}

}
