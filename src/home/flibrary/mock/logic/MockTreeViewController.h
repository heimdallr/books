#pragma once

#include <gmock/gmock.h>

#include "interface/logic/ITreeViewController.h"

namespace HomeCompa::Flibrary::Test {

class MockTreeViewControllerObserver : public ITreeViewController::IObserver
{
public: // ITreeViewController::IObserver
	MOCK_METHOD(void, OnModeChanged, (int index), (override));
	MOCK_METHOD(void, OnModelChanged, (QAbstractItemModel * model), (override));
	MOCK_METHOD(void, OnContextMenuTriggered, (const QString& id, const IDataItem::Ptr& item), (override));
	MOCK_METHOD(QAbstractItemModel*, GetModel, (), (const, noexcept, override));
	MOCK_METHOD(QModelIndex, GetCurrentIndex, (), (const, noexcept, override));
	MOCK_METHOD(void, OnRestoreCurrentIdRequested, (), (override));
};

class MockTreeViewController : public ITreeViewController
{
	using GetModeNamesResult = std::vector<std::pair<const char*, int>>;

public: // ITreeViewController
	MOCK_METHOD(const char*, TrContext, (), (const, noexcept, override));
	MOCK_METHOD(GetModeNamesResult, GetModeNames, (), (const, override));
	MOCK_METHOD(void, SetMode, (const QString& mode), (override));
	MOCK_METHOD(void, RestoreCurrentId, (), (override));
	MOCK_METHOD(void, SetCurrentId, (ItemType type, QString id, bool force), (override));
	MOCK_METHOD(int, GetModeIndex, (), (const, override));
	MOCK_METHOD(ItemType, GetItemType, (), (const, noexcept, override));
	MOCK_METHOD(QString, GetItemName, (), (const, override));
	MOCK_METHOD(ViewMode, GetViewMode, (), (const, noexcept, override));
	MOCK_METHOD(void, RequestContextMenu, (const QModelIndex& index, RequestContextMenuOptions options, RequestContextMenuCallback callback), (override));
	MOCK_METHOD(void, OnContextMenuTriggered, (QAbstractItemModel * model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item), (override));
	MOCK_METHOD(void, OnDoubleClicked, (const QModelIndex& index), (const, override));
	MOCK_METHOD(CreateNewItem, GetNewItemCreator, (), (const, override));
	MOCK_METHOD(RemoveItems, GetRemoveItems, (), (const, override));
	MOCK_METHOD(const QString&, GetNavigationId, (), (const, noexcept, override));
	MOCK_METHOD(QAbstractItemModel*, GetModel, (), (const, noexcept, override));
	MOCK_METHOD(QModelIndex, GetCurrentIndex, (), (const, noexcept, override));
	MOCK_METHOD(void, RegisterObserver, (IObserver * observer), (override));
	MOCK_METHOD(void, UnregisterObserver, (IObserver * observer), (override));
};

static_assert(!std::is_abstract_v<MockTreeViewControllerObserver>, "MockTreeViewControllerObserver is abstract!");
static_assert(!std::is_abstract_v<MockTreeViewController>, "MockTreeViewController is abstract!");

} // namespace HomeCompa::Flibrary::Test
