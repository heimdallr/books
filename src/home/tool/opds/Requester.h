#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/IRequester.h"

class QIODevice;

namespace HomeCompa::Flibrary {
class IAnnotationController;
class ICollectionProvider;
class IDatabaseController;
}

namespace HomeCompa::Opds {

class Requester : virtual public IRequester
{
	NON_COPY_MOVABLE(Requester)

public:
	Requester(std::shared_ptr<Flibrary::ICollectionProvider> collectionProvider
		, std::shared_ptr<Flibrary::IDatabaseController> databaseController
		, std::shared_ptr<Flibrary::IAnnotationController> annotationController
	);
	~Requester() override;

private: // IRequester
	QByteArray GetRoot() const override;
	QByteArray GetBookInfo(const QString & bookId) const override;
	QByteArray GetCoverThumbnail(const QString & bookId) const override;
	QByteArray GetBook(const QString & bookId) const override;
	QByteArray GetBookZip(const QString & bookId) const override;

#define OPDS_ROOT_ITEM(NAME) QByteArray Get##NAME##Navigation(const QString & value) const override;
		OPDS_ROOT_ITEMS_X_MACRO
#undef  OPDS_ROOT_ITEM

#define OPDS_ROOT_ITEM(NAME) QByteArray Get##NAME##Books(const QString & navigationId, const QString & value) const override;
		OPDS_ROOT_ITEMS_X_MACRO
#undef  OPDS_ROOT_ITEM

private:
	template<typename NavigationGetter, typename... ARGS>
	QByteArray GetImpl(NavigationGetter getter, const ARGS &... args) const;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
