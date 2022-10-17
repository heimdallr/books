#pragma once

#include "fnd/observer.h"

namespace HomeCompa::Flibrary {

class ModelController;

class ModelControllerObserver : public Observer
{
public:
	virtual ~ModelControllerObserver() = default;

public:
	virtual void HandleCurrentIndexChanged(ModelController *, int index) = 0;
	virtual void OnKeyPressed(int key, int modifiers) = 0;
};

}
