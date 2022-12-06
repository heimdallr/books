#pragma once

#include "fnd/observer.h"

namespace HomeCompa::Flibrary {

struct Book;

class BooksModelControllerObserver
	: public Observer
{
public:
	virtual ~BooksModelControllerObserver() = default;
	virtual void HandleBookChanged(const Book & book) = 0;
	virtual void HandleModelReset() = 0;
};

}
