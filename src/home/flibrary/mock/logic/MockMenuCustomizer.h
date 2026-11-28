#pragma once

#include <gmock/gmock.h>

#include "interface/logic/IMenuCustomizer.h"

namespace HomeCompa::Flibrary::Test {

class MockMenuCustomizer : public IMenuCustomizer
{
	using SetIconResult = std::expected<void, QString>;

public: // IMenuCustomizer
	MOCK_METHOD(IDataItem::Ptr, GetRootDataItem, (), (const, noexcept, override));
	MOCK_METHOD(ItemAbility, GetAbilities, (const QString& key), (const override));
	MOCK_METHOD(QString, GetHotkey, (const QString& key), (const override));
	MOCK_METHOD(QVariant, GetIcon, (const QString& key), (const override));
	MOCK_METHOD(QVariant, IsHidden, (const QString& key), (const override));
	MOCK_METHOD(QVariant, AddedToToolbar, (const QString& key), (const override));
	MOCK_METHOD(void, UpdateItems, (), (override));
	MOCK_METHOD(void, Add, (const QString& rootKey, QWidget& widget, const QString& title), (override));
	MOCK_METHOD(void, Add, (const QString& rootKey, QMenuBar& menuBar, const QString& title), (override));
	MOCK_METHOD(void, Add, (const QString& rootKey, QComboBox& comboBox, const QString& title), (override));
	MOCK_METHOD(QString, SetHotkey, (const QString& key, const QString& shortCut), (override));
	MOCK_METHOD(void, Hide, (const QString& key, bool hidden), (override));
	MOCK_METHOD(void, AddToToolbar, (const QString& key, bool add), (override));
	MOCK_METHOD(SetIconResult, SetIcon, (const QString& key, const QString& path), (override));
	MOCK_METHOD(void, SetToolbarController, (IToolbarController * toolbarController), (noexcept, override));
	MOCK_METHOD(void, RegisterObserver, (IObserver * observer), (override));
	MOCK_METHOD(void, UnregisterObserver, (IObserver * observer), (override));
};

static_assert(!std::is_abstract_v<MockMenuCustomizer>, "MockMenuCustomizer is abstract!");

} // namespace HomeCompa::Flibrary::Test
