#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IModelProvider.h"

namespace HomeCompa::Flibrary::Test {

class MockModelProvider : public IModelProvider
{
public: // ITreeViewController::IObserver
	MOCK_METHOD(void, OnModeChanged, (int index), (override));
	MOCK_METHOD(void, OnModelChanged, (QAbstractItemModel * model), (override));
	MOCK_METHOD(void, OnContextMenuTriggered, (const QString& id, const IDataItem::Ptr& item), (override));
	MOCK_METHOD(QAbstractItemModel*, GetModel, (), (const, noexcept, override));
	MOCK_METHOD(QModelIndex, GetCurrentIndex, (), (const, noexcept, override));
	MOCK_METHOD(void, OnRestoreCurrentIdRequested, (), (override));

public: // IModelProvider
	MOCK_METHOD(std::shared_ptr<QAbstractItemModel>, CreateListModel, (IDataItem::Ptr data), (const, override));
	MOCK_METHOD(std::shared_ptr<QAbstractItemModel>, CreateTreeModel, (IDataItem::Ptr data), (const, override));
	MOCK_METHOD(std::shared_ptr<QAbstractItemModel>, CreateBookListModel, (IDataItem::Ptr data), (const, override));
	MOCK_METHOD(std::shared_ptr<QAbstractItemModel>, CreateBookTreeModel, (IDataItem::Ptr data), (const, override));
	MOCK_METHOD(std::shared_ptr<QAbstractItemModel>, CreateAuthorsListModel, (IDataItem::Ptr data), (const, override));
	MOCK_METHOD(std::shared_ptr<QAbstractItemModel>, CreateSearchListModel, (IDataItem::Ptr data), (const, override));
	MOCK_METHOD(std::shared_ptr<QAbstractItemModel>, CreateScriptModel, (), (const, override));
	MOCK_METHOD(std::shared_ptr<QAbstractItemModel>, CreateScriptCommandModel, (), (const, override));
	MOCK_METHOD(std::shared_ptr<QAbstractItemModel>, CreateAuthorReviewModel, (), (const, override));
	MOCK_METHOD(std::shared_ptr<QAbstractItemModel>, CreateImageModel, (), (const, override));
	MOCK_METHOD(std::shared_ptr<QAbstractItemModel>, CreateFilterListModel, (IDataItem::Ptr data), (const, override));
	MOCK_METHOD(std::shared_ptr<QAbstractItemModel>, CreateFilterTreeModel, (IDataItem::Ptr data), (const, override));
	MOCK_METHOD(IDataItem::Ptr, GetData, (), (const, noexcept, override));
	MOCK_METHOD(std::shared_ptr<QAbstractItemModel>, GetSourceModel, (), (const, noexcept, override));
	MOCK_METHOD(std::shared_ptr<const ILibRateProvider>, GetLibRateProvider, (), (const, override));
};

static_assert(!std::is_abstract_v<MockModelProvider>, "MockModelProvider is abstract!");

} // namespace HomeCompa::Flibrary::Test
