#pragma once

#include "fnd/observer.h"

class QAbstractItemDelegate;
class QModelIndex;

namespace HomeCompa::Flibrary
{

class ITreeViewDelegate // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		virtual void OnLineHeightChanged(int height) = 0;
		virtual void OnButtonClicked(const QModelIndex&) = 0;
	};

public:
	virtual ~ITreeViewDelegate() = default;

	virtual QAbstractItemDelegate* GetDelegate() noexcept = 0;
	virtual void OnModelChanged() = 0;
	virtual void SetEnabled(bool enabled) noexcept = 0;

	virtual void RegisterObserver(IObserver* observer) = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

}
