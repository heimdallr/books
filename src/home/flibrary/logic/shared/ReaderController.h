#pragma once

#include <functional>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/IReaderController.h"

class QString;

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class ReaderController : virtual public IReaderController
{
	NON_COPY_MOVABLE(ReaderController)

public:
	ReaderController(const std::shared_ptr<const class ILogicFactory>& logicFactory
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<class ICollectionController> collectionController
		, std::shared_ptr<class IUiFactory> uiFactory
		, std::shared_ptr<class IDatabaseUser> databaseUser
	);
	~ReaderController();

public: // IReaderController
	void Read(const QString & folderName, QString fileName, Callback callback) const override;
	void Read(long long id) const override;
	void ReadRandomBook(QString lang) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
