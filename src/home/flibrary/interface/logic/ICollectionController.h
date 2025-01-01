#pragma once

#include "interface/logic/ICollectionProvider.h"

namespace HomeCompa::Flibrary {

class ICollectionController
	: virtual public ICollectionProvider
{
public:
	virtual void AddCollection(const std::filesystem::path & inpx) = 0;
	virtual void RemoveCollection() = 0;

	virtual void SetActiveCollection(const QString & id) = 0;
	virtual void OnInpxUpdateFound(const Collection & updatedCollection) = 0;
};

}
