#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/ICoverCache.h"
#include "interface/IReactAppRequester.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseController.h"

namespace HomeCompa::Opds
{

class ReactAppRequester : virtual public IReactAppRequester
{
	NON_COPY_MOVABLE(ReactAppRequester)

public:
	ReactAppRequester(std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
	                  std::shared_ptr<const Flibrary::IDatabaseController> databaseController,
	                  std::shared_ptr<const ICoverCache> coverCache,
	                  std::shared_ptr<Flibrary::IAnnotationController> annotationController);
	~ReactAppRequester() override;

private: // IReactAppRequester
#define OPDS_GET_BOOKS_API_ITEM(NAME, _) QByteArray NAME(const QString& value) const override;
	OPDS_GET_BOOKS_API_ITEMS_X_MACRO
#undef OPDS_GET_BOOKS_API_ITEM

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
