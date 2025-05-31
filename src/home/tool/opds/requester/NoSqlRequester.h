#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/ICoverCache.h"
#include "interface/INoSqlRequester.h"
#include "interface/logic/IAnnotationController.h"

namespace HomeCompa::Opds
{

class NoSqlRequester final : virtual public INoSqlRequester
{
	NON_COPY_MOVABLE(NoSqlRequester)

public:
	NoSqlRequester(std::shared_ptr<const ICoverCache> coverCache, std::shared_ptr<Flibrary::IAnnotationController> annotationController);
	~NoSqlRequester() override;

private:
	QByteArray GetCover(const QString& bookId) const override;
	QByteArray GetCoverThumbnail(const QString& bookId) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
