#pragma once

#include "fnd/memory.h"

namespace HomeCompa::Flibrary {

class GuiController
{
public:
	GuiController();
	~GuiController();

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
