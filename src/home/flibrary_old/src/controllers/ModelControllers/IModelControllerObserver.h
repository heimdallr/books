#pragma once

#include "fnd/observer.h"

namespace HomeCompa::Flibrary {

class ModelController;

class IModelControllerObserver : public Observer
{
public:
	virtual ~IModelControllerObserver() = default;

public:
	virtual void HandleCurrentIndexChanged(ModelController *, int index) = 0;
	virtual void HandleClicked(ModelController *) = 0;
};

}
