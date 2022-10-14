#pragma once

namespace HomeCompa::Flibrary {

class ModelController;

class ModelControllerObserver
{
public:
	virtual ~ModelControllerObserver() = default;

public:
	virtual void HandleCurrentIndexChanged(ModelController *, int index) = 0;
};

}
