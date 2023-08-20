#include "di_app.h"

#include <Hypodermic/Hypodermic.h>

#include "logic/di_logic.h"
#include "ui/di_ui.h"

namespace HomeCompa::Flibrary {

std::shared_ptr<Hypodermic::Container> DiInit(Hypodermic::ContainerBuilder & builder, const std::shared_ptr<Hypodermic::Container> & container)
{
	DiLogic(builder, container);
	DiUi(builder, container);

	return builder.build();
}

}
