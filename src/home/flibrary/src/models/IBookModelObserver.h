#pragma once

#include <QMetaType>

#include "IModelObserver.h"

namespace HomeCompa::Flibrary {

struct Book;

class IBookModelObserver : virtual public IModelObserver
{
public:
	virtual void HandleBookRemoved(const std::vector<std::reference_wrapper<const Book>> & books) = 0;
};

}

Q_DECLARE_METATYPE(HomeCompa::Flibrary::IBookModelObserver *);
