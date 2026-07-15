#pragma once
#include "fnd/observer.h"

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
		virtual void OnNavigationModelChanged(std::shared_ptr<QAbstractItemModel> model) = 0;
	};

public:
	virtual ~IImageViewerController() = default;

	virtual QAbstractItemModel* GetImageModel() noexcept            = 0;
	virtual void                SetFolder(const QModelIndex& index) = 0;
	virtual void                SetImageSize(int value)             = 0;

	virtual void RegisterObserver(IObserver* observer)   = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

} // namespace HomeCompa::Flibrary
