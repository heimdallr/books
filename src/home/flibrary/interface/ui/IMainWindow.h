#pragma once
#include "interface/logic/ISingleInstanceController.h"

namespace HomeCompa::Flibrary
{

struct Collection;

class IMainWindow : public ISingleInstanceController::IObserver
{
public:
	virtual void CreateCollection(Collection collection) = 0;
	virtual void Show() = 0;
};

}
