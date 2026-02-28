#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IBookExtractor.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseUser.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class BookExtractor final : virtual public IBookExtractor
{
	NON_COPY_MOVABLE(BookExtractor)

public:
	BookExtractor(std::shared_ptr<const ISettings> settings, std::shared_ptr<const ICollectionProvider> collectionProvider, std::shared_ptr<const IDatabaseUser> databaseUser);
	~BookExtractor() override;

private: // IBookExtractor
	QString                     GetFileName(const QString& bookId) const override;
	QString                     GetFileName(const Util::ExtractedBook& book) const override;
	Util::ExtractedBook         GetExtractedBook(const QString& bookId) const override;
	Util::ExtractedBook::Author GetExtractedBookAuthor(const QString& bookId) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
