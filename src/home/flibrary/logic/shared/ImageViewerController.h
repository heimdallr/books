#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IImageViewerController.h"
#include "interface/logic/IModelProvider.h"

#include "BooksContextMenuProvider.h"

namespace HomeCompa::Flibrary
{

class ImageViewerController final : public IImageViewerController
{
	NON_COPY_MOVABLE(ImageViewerController)

public:
	ImageViewerController(std::shared_ptr<const IModelProvider> modelProvider, std::shared_ptr<IBookInfoProvider> bookInfoProvider);
	~ImageViewerController() override;

private: // IImageViewerController
	QAbstractItemModel* GetImageModel() noexcept override;
	void                SetImageSize(int value) override;
	void                PrepareImage(const QModelIndex& index) override;
	void                RequestImage(const QModelIndex& index) override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
