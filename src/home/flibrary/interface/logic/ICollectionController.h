#pragma once

#include "interface/logic/ICollectionProvider.h"

namespace HomeCompa::Flibrary
{

class ICollectionController : virtual public ICollectionProvider
{
public:
	virtual void AddCollection(const std::filesystem::path& inpxDir) = 0;
	virtual void CreateCollection(const Collection& collection)      = 0;
	virtual void RescanCollectionFolder()                            = 0;
	virtual void RemoveCollection()                                  = 0;

	virtual void SetActiveCollection(const QString& id)                   = 0;
	virtual bool OnInpxUpdateChecked(const Collection& updatedCollection) = 0;

	virtual void AllowDestructiveOperation(bool value) = 0;

	virtual Collection::Ptr CreateCollection(QString name, QString database, QString folder, QString additionalFolder, QString inpx) = 0;
};

}
