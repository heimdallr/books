#pragma once

#include <QMetaType>

#include "ModelObserver.h"

namespace HomeCompa::Flibrary {

struct Book;

class BookModelObserver : virtual public ModelObserver
{
public:
	virtual void HandleBookRemoved(const std::vector<std::reference_wrapper<const Book>> & books) = 0;
};

}

Q_DECLARE_METATYPE(HomeCompa::Flibrary::BookModelObserver *);
