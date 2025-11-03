#pragma once
#include "interface/logic/ISingleInstanceController.h"

namespace HomeCompa::Flibrary
{

class IMainWindow : public ISingleInstanceController::IObserver
{
public:
	virtual void Show() = 0;
};

}
