#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/IBookExtractor.h"
#include "interface/ICoverCache.h"
#include "interface/INoSqlRequester.h"
#include "interface/IRequester.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/IAuthorAnnotationController.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseController.h"

#include "util/ISettings.h"

class QIODevice;

namespace HomeCompa::Opds
{

class Requester final : virtual public IRequester
{
	NON_COPY_MOVABLE(Requester)

public:
	Requester(std::shared_ptr<const ISettings> settings,
	          std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
	          std::shared_ptr<const Flibrary::IDatabaseController> databaseController,
	          std::shared_ptr<const Flibrary::IAuthorAnnotationController> authorAnnotationController,
	          std::shared_ptr<const ICoverCache> coverCache,
	          std::shared_ptr<const IBookExtractor> bookExtractor,
	          std::shared_ptr<const INoSqlRequester> noSqlRequester,
	          std::shared_ptr<Flibrary::IAnnotationController> annotationController);
	~Requester() override;

private: // IRequester
	QByteArray GetRoot(const QString& root, const Parameters& parameters) const override;
	QByteArray GetBooks(const QString& root, const Parameters& parameters) const override;

#define OPDS_ROOT_ITEM(NAME) QByteArray Get##NAME(const QString& root, const Parameters& parameters) const override;
	OPDS_ROOT_ITEMS_X_MACRO
#undef OPDS_ROOT_ITEM

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Opds
