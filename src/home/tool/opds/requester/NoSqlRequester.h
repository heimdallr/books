#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/ICoverCache.h"
#include "interface/INoSqlRequester.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/IBookExtractor.h"
#include "interface/logic/ICollectionProvider.h"

namespace HomeCompa::Opds
{

class NoSqlRequester final : virtual public INoSqlRequester
{
	NON_COPY_MOVABLE(NoSqlRequester)

public:
	NoSqlRequester(
		std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
		std::shared_ptr<const Flibrary::IBookExtractor>      bookExtractor,
		std::shared_ptr<const ICoverCache>                   coverCache,
		std::shared_ptr<Flibrary::IAnnotationController>     annotationController
	);
	~NoSqlRequester() override;

private:
	QByteArray                     GetCover(const QString& bookId) const override;
	QByteArray                     GetCoverThumbnail(const QString& bookId) const override;
	std::pair<QString, QByteArray> GetBook(const QString& bookId, bool restoreImages) const override;
	std::pair<QString, QByteArray> GetBookZip(const QString& bookId, bool restoreImages) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
