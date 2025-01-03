#pragma once

#include <functional>
#include <QString>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa::Flibrary {

class InpxCollectionExtractor
{
	NON_COPY_MOVABLE(InpxCollectionExtractor)

public:
	using Callback = std::function<void(bool)>;

public:
	InpxCollectionExtractor(std::shared_ptr<class ICollectionController> collectionController
		, std::shared_ptr<class IBooksExtractorProgressController> progressController
		, std::shared_ptr<class DatabaseUser> databaseUser
		, const std::shared_ptr<const class ILogicFactory>& logicFactory
	);
	~InpxCollectionExtractor();

public:
	void ExtractAsInpxCollection(QString folder, const std::vector<QString> & idList, const class DataProvider & dataProvider, Callback callback);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
