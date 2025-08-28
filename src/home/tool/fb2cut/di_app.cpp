#include "di_app.h"

#include "Hypodermic/Hypodermic.h"
#include "gutil/di_gutil.h"
#include "logic/di_logic.h"

namespace HomeCompa::fb2cut
{

void DiInit(Hypodermic::ContainerBuilder& builder, std::shared_ptr<Hypodermic::Container>& container)
{
	Util::DiGUtil(builder, container);
	Flibrary::DiLogic(builder, container);

	container = builder.build();
}

}
