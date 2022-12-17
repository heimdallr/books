#pragma once

#include "fnd/observer.h"

namespace HomeCompa::Flibrary {

class ModelObserver : public Observer
{
public:
	virtual ~ModelObserver() = default;

public:
	virtual void HandleModelItemFound(int index) = 0;
	virtual void HandleItemClicked(int index) = 0;
	virtual void HandleItemDoubleClicked(int index) = 0;
	virtual void HandleInvalidated() = 0;
};

}
