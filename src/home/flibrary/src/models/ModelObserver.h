#pragma once

#include <QMetaType>

#include "fnd/observer.h"

namespace HomeCompa::Flibrary {

struct Book;

class ModelObserver : public Observer
{
public:
	virtual ~ModelObserver() = default;

public:
	virtual void HandleModelItemFound(int index) = 0;
	virtual void HandleItemClicked(int index) = 0;
	virtual void HandleInvalidated() = 0;
	virtual void HandleBookRemoved(const Book & book) = 0;
};

}

Q_DECLARE_METATYPE(HomeCompa::Flibrary::ModelObserver *);
