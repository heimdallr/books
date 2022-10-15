#pragma once

namespace HomeCompa::Flibrary {

class ModelObserver
{
public:
	virtual ~ModelObserver() = default;

public:
	virtual void HandleModelItemFound(int index) = 0;
};

}