#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IImageViewerController.h"
#include "interface/logic/IModelProvider.h"

namespace HomeCompa::Flibrary
{

class ImageViewerController final : public IImageViewerController
{
	NON_COPY_MOVABLE(ImageViewerController)

public:
	explicit ImageViewerController(std::shared_ptr<const IModelProvider> modelProvider);
	~ImageViewerController() override;

private: // IImageViewerController
	QAbstractItemModel* GetImageModel() noexcept override;
	void                SetImageSize(int value) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
