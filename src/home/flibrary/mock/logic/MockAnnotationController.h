#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IAnnotationController.h"

namespace HomeCompa::Flibrary::Test {

class MockAnnotationControllerDataProvider : public IAnnotationController::IDataProvider
{
	MOCK_METHOD(const IDataItem&, GetBook, (), (const, noexcept, override));
	MOCK_METHOD(const IDataItem&, GetSeries, (), (const, noexcept, override));
	MOCK_METHOD(const IDataItem&, GetAuthors, (), (const, noexcept, override));
	MOCK_METHOD(const IDataItem&, GetGenres, (), (const, noexcept, override));
	MOCK_METHOD(const IDataItem&, GetGroups, (), (const, noexcept, override));
	MOCK_METHOD(const IDataItem&, GetKeywords, (), (const, noexcept, override));
	MOCK_METHOD(const IDataItem&, GetFolder, (), (const, noexcept, override));
	MOCK_METHOD(const IDataItem&, GetUpdate, (), (const, noexcept, override));
	MOCK_METHOD(const QString&, GetError, (), (const, noexcept, override));
	MOCK_METHOD(const QString&, GetAnnotation, (), (const, noexcept, override));
	MOCK_METHOD(const QString&, GetEpigraph, (), (const, noexcept, override));
	MOCK_METHOD(const QString&, GetEpigraphAuthor, (), (const, noexcept, override));
	MOCK_METHOD(const QString&, GetLanguage, (), (const, noexcept, override));
	MOCK_METHOD(const QString&, GetSourceLanguage, (), (const, noexcept, override));
	MOCK_METHOD(const QString&, GetSourceLibrary, (), (const, noexcept, override));
	MOCK_METHOD(const std::vector<QString>&, GetFb2Keywords, (), (const, noexcept, override));
	MOCK_METHOD(const Covers&, GetCovers, (), (const, noexcept, override));
	MOCK_METHOD(size_t, GetTextSize, (), (const, noexcept, override));
	MOCK_METHOD(size_t, GetWordCount, (), (const, noexcept, override));
	MOCK_METHOD(IDataItem::Ptr, GetContent, (), (const, noexcept, override));
	MOCK_METHOD(IDataItem::Ptr, GetDescription, (), (const, noexcept, override));
	MOCK_METHOD(IDataItem::Ptr, GetTranslators, (), (const, noexcept, override));
	MOCK_METHOD(const QString&, GetPublisher, (), (const, noexcept, override));
	MOCK_METHOD(const QString&, GetPublishCity, (), (const, noexcept, override));
	MOCK_METHOD(const QString&, GetPublishYear, (), (const, noexcept, override));
	MOCK_METHOD(const QString&, GetPublishIsbn, (), (const, noexcept, override));
	MOCK_METHOD(const ExportStatistics&, GetExportStatistics, (), (const, noexcept, override));
	MOCK_METHOD(const Reviews&, GetReviews, (), (const, noexcept, override));
	MOCK_METHOD(std::vector<IDataItem::Flags>, GetFlags, (NavigationMode navigationMode, const std::vector<QString>& ids), (const, override));
};

class MockAnnotationController : public IAnnotationController
{
public: // IAnnotationController
	MOCK_METHOD(void, SetNavigationMode, (NavigationMode navigationMode), (noexcept, override));
	MOCK_METHOD(void, SetCurrentBookId, (QString bookId, bool extractNow), (override));
	MOCK_METHOD(QString, CreateAnnotation, (const IDataProvider& dataProvider, const IStrategy& strategy), (const, override));
	MOCK_METHOD(void, ShowJokes, (IJokeRequesterFactory::Implementation impl, bool value), (override));
	MOCK_METHOD(void, ShowReviews, (bool value), (override));
	MOCK_METHOD(void, RegisterObserver, (IObserver * observer), (override));
	MOCK_METHOD(void, UnregisterObserver, (IObserver * observer), (override));
};

static_assert(!std::is_abstract_v<MockAnnotationControllerDataProvider>, "MockAnnotationControllerDataProvider is abstract!");
static_assert(!std::is_abstract_v<MockAnnotationController>, "MockAnnotationController is abstract!");

} // namespace HomeCompa::Flibrary::Test
