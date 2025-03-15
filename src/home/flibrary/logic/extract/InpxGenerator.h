#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IInpxGenerator.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IProgressController.h"

namespace HomeCompa::Flibrary
{

class InpxGenerator : public IInpxGenerator
{
	NON_COPY_MOVABLE(InpxGenerator)

public:
	InpxGenerator(const std::shared_ptr<const ILogicFactory>& logicFactory,
	              std::shared_ptr<const ICollectionProvider> collectionProvider,
	              std::shared_ptr<const IDatabaseUser> databaseUser,
	              std::shared_ptr<IBooksExtractorProgressController> progressController);
	~InpxGenerator() override;

public:
	void ExtractAsInpxCollection(QString folder, const std::vector<QString>& idList, const class IBookInfoProvider& dataProvider, Callback callback) override;
	void GenerateInpx(QString inpxFileName, const std::vector<QString>& idList, const IBookInfoProvider& dataProvider, Callback callback) override;
	void GenerateInpx(QString inpxFileName, Callback callback) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
