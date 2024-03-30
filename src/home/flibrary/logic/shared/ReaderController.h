#pragma once

#include <functional>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QString;

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class ReaderController
{
	NON_COPY_MOVABLE(ReaderController)

public:
	using Callback = std::function<void()>;

public:
	ReaderController(std::shared_ptr<ISettings> settings
		, std::shared_ptr<class ICollectionController> collectionController
		, const std::shared_ptr<const class ILogicFactory>& logicFactory
		, std::shared_ptr<class IUiFactory> uiFactory
	);
	~ReaderController();

public:
	void Read(const QString & folderName, QString fileName, Callback callback) const;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
