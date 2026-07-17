#pragma once

#include "fnd/observer.h"

class QPixmap;
class QModelIndex;
class QAbstractItemModel;
class QString;

namespace HomeCompa::Flibrary
{

class IImageViewerController // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		virtual void OnImageReceived(QPixmap pixmap) = 0;
		virtual void OnCountChanges(int count)       = 0;
	};

public:
	virtual ~IImageViewerController() = default;

	virtual QAbstractItemModel* GetImageModel() noexcept         = 0;
	virtual void                SetImageSize(int value)          = 0;
	virtual void                PrepareImage(const QModelIndex&) = 0;
	virtual void                RequestImage(const QModelIndex&) = 0;

	virtual void RegisterObserver(IObserver* observer)   = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

} // namespace HomeCompa::Flibrary
