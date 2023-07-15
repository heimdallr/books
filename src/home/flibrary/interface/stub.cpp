#include <cassert>

#include "logic/ILogicFactory.h"

namespace HomeCompa::Flibrary {

namespace {

const ILogicFactory * g_logicFactory = nullptr;

}

ILogicFactory::Guard::Guard(const ILogicFactory & logicFactory) noexcept
{
	g_logicFactory = &logicFactory;
}

ILogicFactory::Guard::~Guard()
{
	g_logicFactory = nullptr;
}

const ILogicFactory & ILogicFactory::Instance()
{
	assert(g_logicFactory);
	return *g_logicFactory;
}

}
