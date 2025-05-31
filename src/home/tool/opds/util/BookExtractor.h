#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/IBookExtractor.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseController.h"

#include "util/ISettings.h"

namespace HomeCompa::Opds
{

class BookExtractor final : virtual public IBookExtractor
{
	NON_COPY_MOVABLE(BookExtractor)

public:
	BookExtractor(std::shared_ptr<const ISettings> settings, std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider, std::shared_ptr<const Flibrary::IDatabaseController> databaseController);
	~BookExtractor() override;

private: // IBookExtractor
	QString GetFileName(const QString& bookId) const override;
	QString GetFileName(const Flibrary::ILogicFactory::ExtractedBook& book) const override;
	Flibrary::ILogicFactory::ExtractedBook GetExtractedBook(const QString& bookId) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
