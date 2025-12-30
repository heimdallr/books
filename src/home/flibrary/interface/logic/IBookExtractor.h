#pragma once

#include "interface/logic/ILogicFactory.h"

class QString;

namespace HomeCompa::Flibrary
{

class IBookExtractor // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IBookExtractor() = default;

	virtual QString                              GetFileName(const QString& bookId) const                    = 0;
	virtual QString                              GetFileName(const ILogicFactory::ExtractedBook& book) const = 0;
	virtual ILogicFactory::ExtractedBook         GetExtractedBook(const QString& bookId) const               = 0;
	virtual ILogicFactory::ExtractedBook::Author GetExtractedBookAuthor(const QString& bookId) const         = 0;
};

}
