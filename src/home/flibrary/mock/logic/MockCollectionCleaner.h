#pragma once

#include <gmock/gmock.h>

#include "interface/logic/ICollectionCleaner.h"

namespace HomeCompa::Flibrary::Test {

class MockCollectionCleanerAnalyzeObserver : public ICollectionCleaner::IAnalyzeObserver
{
public: // ICollectionCleaner::IAnalyzeObserver
	MOCK_METHOD(void, AnalyzeFinished, (Util::Remove::Books books), (override));
	MOCK_METHOD(bool, IsPermanently, (), (const, override));
	MOCK_METHOD(bool, NeedDeleteMarkedAsDeleted, (), (const, override));
	MOCK_METHOD(bool, NeedDeleteDuplicates, (), (const, override));
	MOCK_METHOD(bool, NeedDeleteUnrated, (), (const, override));
	MOCK_METHOD(QStringList, GetLanguages, (), (const, override));
	MOCK_METHOD(QStringList, GetGenres, (), (const, override));
	MOCK_METHOD(ICollectionCleaner::CleanGenreMode, GetCleanGenreMode, (), (const, override));
	MOCK_METHOD(std::optional<size_t>, GetMinimumBookSize, (), (const, override));
	MOCK_METHOD(std::optional<size_t>, GetMaximumBookSize, (), (const, override));
	MOCK_METHOD(std::optional<double>, GetMinimumLibRate, (), (const, override));
	MOCK_METHOD(bool, NeedDeleteCompletelyDuplicatedCompilations, (), (const, override));
	MOCK_METHOD(bool, NeedDeleteBooksDuplicatedByCompilations, (), (const, override));
	MOCK_METHOD(void, CompilationInfoExistsResponse, (bool value), (const, override));
};

class MockCollectionCleaner : public ICollectionCleaner
{
public: // ICollectionCleaner
	MOCK_METHOD(void, Remove, (Util::Remove::Books books, Callback callback), (const, override));
	MOCK_METHOD(void, RemovePermanently, (Util::Remove::Books books, Callback callback), (const, override));
	MOCK_METHOD(void, Analyze, (IAnalyzeObserver & callback), (const, override));
	MOCK_METHOD(void, AnalyzeCancel, (), (const, override));
	MOCK_METHOD(void, CompilationInfoExistsRequest, (IAnalyzeObserver & callback), (const, override));
};

static_assert(!std::is_abstract_v<MockCollectionCleanerAnalyzeObserver>, "MockCollectionCleanerAnalyzeObserver is abstract!");
static_assert(!std::is_abstract_v<MockCollectionCleaner>, "MockCollectionCleaner is abstract!");

} // namespace HomeCompa::Flibrary::Test
