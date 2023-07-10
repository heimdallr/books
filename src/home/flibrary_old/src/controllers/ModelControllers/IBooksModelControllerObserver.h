#pragma once

#include "fnd/observer.h"

namespace HomeCompa::Flibrary {

struct Book;

class IBooksModelControllerObserver
	: public Observer
{
public:
	virtual ~IBooksModelControllerObserver() = default;
	virtual void HandleBookChanged(const Book & book) = 0;
	virtual void HandleModelReset() = 0;
};

}
