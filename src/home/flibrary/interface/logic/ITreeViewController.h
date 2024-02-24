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

namespace HomeCompa::Flibrary {

class ITreeViewController  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using RequestContextMenuCallback = std::function<void(const QString & id, const IDataItem::Ptr & item)>;
	enum class RequestContextMenuOptions
	{
		None          = 0,
		IsTree        = 1 << 0,
		HasExpanded   = 1 << 1,
		HasCollapsed  = 1 << 2,
		NodeExpanded  = 1 << 3,
		NodeCollapsed = 1 << 4,
	};

public:
	class IObserver : public Observer
	{
	public:
		virtual void OnModeChanged(int index) = 0;
		virtual void OnModelChanged(QAbstractItemModel * model) = 0;
		virtual void OnContextMenuTriggered(const QString & id, const IDataItem::Ptr & item) = 0;
	};

public:
	virtual ~ITreeViewController() = default;
	[[nodiscard]] virtual const char * TrContext() const noexcept = 0;
	[[nodiscard]] virtual std::vector<const char *> GetModeNames() const = 0;
	virtual void SetMode(const QString & mode) = 0;
	virtual void SetCurrentId(QString id) = 0;
	[[nodiscard]] virtual int GetModeIndex() const = 0;
	[[nodiscard]] virtual enum class ItemType GetItemType() const noexcept = 0;
	[[nodiscard]] virtual enum class ViewMode GetViewMode() const noexcept = 0;
	virtual void RequestContextMenu(const QModelIndex & index, RequestContextMenuOptions options, RequestContextMenuCallback callback) = 0;
	virtual void OnContextMenuTriggered(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item) = 0;
	virtual void OnDoubleClicked(const QModelIndex & index) const = 0;

	virtual void RegisterObserver(IObserver * observer) = 0;
	virtual void UnregisterObserver(IObserver * observer) = 0;
};

}

ENABLE_BITMASK_OPERATORS(HomeCompa::Flibrary::ITreeViewController::RequestContextMenuOptions);
