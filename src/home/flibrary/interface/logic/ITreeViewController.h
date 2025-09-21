#pragma once

#include <functional>
#include <vector>

#include "fnd/EnumBitmask.h"
#include "fnd/observer.h"

#include "IDataItem.h"

class QModelIndex;
class QPoint;
class QAbstractItemModel;
class QString;

namespace HomeCompa::Flibrary
{

class ITreeViewController // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using RequestContextMenuCallback = std::function<void(const QString& id, const IDataItem::Ptr& item)>;
	using CreateNewItem = std::function<void()>;
	using RemoveItems = std::function<void(const QList<QModelIndex>& indexList)>;
	enum class RequestContextMenuOptions
	{
		None = 0,
		// clang-format off
		IsTree                     = 1 << 0,
		HasExpanded                = 1 << 1,
		HasCollapsed               = 1 << 2,
		NodeExpanded               = 1 << 3,
		NodeCollapsed              = 1 << 4,
		HasSelection               = 1 << 5,
		AllowDestructiveOperations = 1 << 6,
		IsArchive                  = 1 << 7,
		ShowRemoved                = 1 << 8,
		UniFilterEnabled           = 1 << 9,
		// clang-format on
	};

public:
	class IObserver : public Observer
	{
	public:
		virtual void OnModeChanged(int index) = 0;
		virtual void OnModelChanged(QAbstractItemModel* model) = 0;
		virtual void OnContextMenuTriggered(const QString& id, const IDataItem::Ptr& item) = 0;
	};

public:
	virtual ~ITreeViewController() = default;
	[[nodiscard]] virtual const char* TrContext() const noexcept = 0;
	[[nodiscard]] virtual std::vector<std::pair<const char*, int>> GetModeNames() const = 0;
	virtual void SetMode(const QString& mode) = 0;
	virtual void SetCurrentId(ItemType type, QString id) = 0;
	[[nodiscard]] virtual int GetModeIndex() const = 0;
	[[nodiscard]] virtual ItemType GetItemType() const noexcept = 0;
	[[nodiscard]] virtual QString GetItemName() const = 0;
	[[nodiscard]] virtual enum class ViewMode GetViewMode() const noexcept = 0;
	virtual void RequestContextMenu(const QModelIndex& index, RequestContextMenuOptions options, RequestContextMenuCallback callback) = 0;
	virtual void OnContextMenuTriggered(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item) = 0;
	virtual void OnDoubleClicked(const QModelIndex& index) const = 0;
	virtual CreateNewItem GetNewItemCreator() const = 0;
	virtual RemoveItems GetRemoveItems() const = 0;
	virtual const QString& GetNavigationId() const noexcept = 0;

	virtual void RegisterObserver(IObserver* observer) = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

} // namespace HomeCompa::Flibrary

ENABLE_BITMASK_OPERATORS(HomeCompa::Flibrary::ITreeViewController::RequestContextMenuOptions);
