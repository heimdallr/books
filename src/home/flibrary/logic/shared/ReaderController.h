#pragma once

#include <functional>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ICollectionController.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IReaderController.h"
#include "interface/ui/IUiFactory.h"

#include "util/ISettings.h"

class QString;

namespace HomeCompa::Flibrary
{

class ReaderController : virtual public IReaderController
{
	NON_COPY_MOVABLE(ReaderController)

public:
	ReaderController(
		const std::shared_ptr<const ILogicFactory>& logicFactory,
		std::shared_ptr<ISettings>                  settings,
		std::shared_ptr<ICollectionController>      collectionController,
		std::shared_ptr<IUiFactory>                 uiFactory,
		std::shared_ptr<IDatabaseUser>              databaseUser
	);
	~ReaderController() override;

public: // IReaderController
	void Read(long long id) const override;
	void ReadRandomBook(QString lang) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
