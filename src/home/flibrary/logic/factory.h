#pragma once

#include "fnd/memory.h"
#include "interface/logic/ILogicFactory.h"

namespace HomeCompa::Flibrary {

class LogicFactory : virtual public ILogicFactory
{
public:
	LogicFactory();
	~LogicFactory() override;

private: // ILogicFactory

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
