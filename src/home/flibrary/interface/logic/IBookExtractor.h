#pragma once

#include "interface/logic/ILogicFactory.h"

class QString;

namespace HomeCompa::Flibrary
{

class IBookExtractor // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IBookExtractor() = default;

	virtual QString                     GetFileName(const QString& bookId) const            = 0;
	virtual QString                     GetFileName(const Util::ExtractedBook& book) const  = 0;
	virtual Util::ExtractedBook         GetExtractedBook(const QString& bookId) const       = 0;
	virtual Util::ExtractedBook::Author GetExtractedBookAuthor(const QString& bookId) const = 0;
};

}
