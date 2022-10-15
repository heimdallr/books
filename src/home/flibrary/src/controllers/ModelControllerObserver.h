#pragma once

namespace HomeCompa::Flibrary {

class ModelController;

class ModelControllerObserver
{
public:
	virtual ~ModelControllerObserver() = default;

public:
	virtual void HandleClicked(ModelController *, int index) = 0;
	virtual void HandleCurrentIndexChanged(ModelController *, int index) = 0;
};

}
