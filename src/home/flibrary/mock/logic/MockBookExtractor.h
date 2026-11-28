#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IBookExtractor.h"

namespace HomeCompa::Flibrary::Test {

class MockBookExtractor : public IBookExtractor
{
public: // IBookExtractor
	MOCK_METHOD(QString, GetFileName, (const Util::ExtractedBook& book), (const, override));
	MOCK_METHOD(QString, GetFileName, (const QString& bookId), (const, override));
	MOCK_METHOD(Util::ExtractedBook, GetExtractedBook, (const QString& bookId), (const, override));
	MOCK_METHOD(Util::ExtractedBook::Author, GetExtractedBookAuthor, (const QString& bookId), (const, override));
};

static_assert(!std::is_abstract_v<MockBookExtractor>, "MockBookExtractor is abstract!");

} // namespace HomeCompa::Flibrary::Test
