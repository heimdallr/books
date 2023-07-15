#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/ICollectionController.h"

#include "logicLib.h"

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

struct Collection;

class LOGIC_API CollectionController final
	: virtual public ICollectionController
{
	NON_COPY_MOVABLE(CollectionController)

public:
	CollectionController(std::shared_ptr<ISettings> settings, std::shared_ptr<class IUiFactory> uiFactory);
	~CollectionController() override;

public: // ICollectionController
	[[nodiscard]] bool AddCollection() override;
	[[nodiscard]] bool IsEmpty() const noexcept override;
	[[nodiscard]] bool IsCollectionNameExists(const QString & name) const override;
	[[nodiscard]] QString GetCollectionDatabaseName(const QString & databaseFileName) const override;
	[[nodiscard]] bool IsCollectionFolderHasInpx(const QString & folder) const override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
