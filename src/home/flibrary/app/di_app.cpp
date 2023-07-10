#include "di_app.h"

#include <Hypodermic/Hypodermic.h>

#include "logic/di_logic.h"
#include "ui/di_ui.h"

namespace HomeCompa::Flibrary {

std::shared_ptr<Hypodermic::Container> di_init(Hypodermic::ContainerBuilder & builder, const std::shared_ptr<Hypodermic::Container> & container)
{
	di_logic(builder, container);
	di_ui(builder, container);

	return builder.build();
}

}
