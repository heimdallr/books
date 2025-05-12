#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/IRequester.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/IAuthorAnnotationController.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseController.h"

#include "util/ISettings.h"

class QIODevice;

namespace HomeCompa::Opds
{

class Requester : virtual public IRequester
{
	NON_COPY_MOVABLE(Requester)

public:
	Requester(std::shared_ptr<const ISettings> settings,
	          std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
	          std::shared_ptr<const Flibrary::IDatabaseController> databaseController,
	          std::shared_ptr<const Flibrary::IAuthorAnnotationController> authorAnnotationController,
	          std::shared_ptr<Flibrary::IAnnotationController> annotationController);
	~Requester() override;

private: // IRequester
	QByteArray GetRoot(const QString& root, const QString& self) const override;
	QByteArray GetBookInfo(const QString& root, const QString& self, const QString& bookId) const override;
	QByteArray GetCover(const QString& root, const QString& self, const QString& bookId) const override;
	QByteArray GetCoverThumbnail(const QString& root, const QString& self, const QString& bookId) const override;
	std::pair<QString, QByteArray> GetBook(const QString& root, const QString& self, const QString& bookId, bool transliterate) const override;
	std::pair<QString, QByteArray> GetBookZip(const QString& root, const QString& self, const QString& bookId, bool transliterate) const override;
	QByteArray GetBookText(const QString& root, const QString& bookId) const override;
	QByteArray Search(const QString& root, const QString& self, const QString& searchTerms, const QString& start) const override;

#define OPDS_ROOT_ITEM(NAME) QByteArray Get##NAME##Navigation(const QString& root, const QString& self, const QString& value) const override;
	OPDS_ROOT_ITEMS_X_MACRO
#undef OPDS_ROOT_ITEM

#define OPDS_ROOT_ITEM(NAME) QByteArray Get##NAME##Authors(const QString& root, const QString& self, const QString& navigationId, const QString& value) const override;
	OPDS_ROOT_ITEMS_X_MACRO
#undef OPDS_ROOT_ITEM

#define OPDS_ROOT_ITEM(NAME) QByteArray Get##NAME##AuthorBooks(const QString& root, const QString& self, const QString& navigationId, const QString& authorId, const QString& value) const override;
	OPDS_ROOT_ITEMS_X_MACRO
#undef OPDS_ROOT_ITEM

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Opds
