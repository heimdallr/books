#pragma once

#include "fnd/observer.h"

namespace HomeCompa::Flibrary {

class BooksModelControllerObserver
	: public Observer
{
public:
	virtual ~BooksModelControllerObserver() = default;
	virtual void HandleBookChanged(const std::string & folder, const std::string & file) = 0;
};

}
