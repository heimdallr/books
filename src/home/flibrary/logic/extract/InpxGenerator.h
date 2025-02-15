#pragma once

#include <functional>

#include <QString>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IInpxGenerator.h"

namespace HomeCompa::Flibrary
{

class InpxGenerator : public IInpxGenerator
{
	NON_COPY_MOVABLE(InpxGenerator)

public:
	InpxGenerator(const std::shared_ptr<const class ILogicFactory>& logicFactory,
	              std::shared_ptr<class ICollectionController> collectionController,
	              std::shared_ptr<class IBooksExtractorProgressController> progressController,
	              std::shared_ptr<class IDatabaseUser> databaseUser);
	~InpxGenerator() override;

public:
	void ExtractAsInpxCollection(QString folder, const std::vector<QString>& idList, const IBookInfoProvider& dataProvider, Callback callback) override;
	void GenerateInpx(QString inpxFileName, const std::vector<QString>& idList, const IBookInfoProvider& dataProvider, Callback callback) override;
	void GenerateInpx(QString inpxFileName, Callback callback) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
