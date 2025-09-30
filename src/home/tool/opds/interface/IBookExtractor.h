#pragma once

#include "interface/logic/ILogicFactory.h"

class QString;

namespace HomeCompa::Opds
{

class IBookExtractor // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IBookExtractor() = default;

	virtual QString                                GetFileName(const QString& bookId) const                              = 0;
	virtual QString                                GetFileName(const Flibrary::ILogicFactory::ExtractedBook& book) const = 0;
	virtual Flibrary::ILogicFactory::ExtractedBook GetExtractedBook(const QString& bookId) const                         = 0;
};

}
