#pragma once

#include "interface/logic/ICollectionProvider.h"

namespace HomeCompa::Inpx
{
enum class CheckForUpdateResult;
}

namespace HomeCompa::Flibrary
{

class ICollectionController : virtual public ICollectionProvider
{
public:
	virtual void AddCollection(const std::filesystem::path& inpxDir) = 0;
	virtual void RemoveCollection() = 0;

	virtual void SetActiveCollection(const QString& id) = 0;
	virtual void OnInpxUpdateChecked(const Collection& updatedCollection, Inpx::CheckForUpdateResult result) = 0;

	virtual void AllowDestructiveOperation(bool value) = 0;
};

}
