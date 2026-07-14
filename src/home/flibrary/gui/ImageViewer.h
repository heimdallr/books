#pragma once

#include <QWidget>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IImageViewerController.h"
#include "interface/ui/IUiFactory.h"

#include "utilgui/ItemViewToolTipper.h"
#include "utilgui/ScrollBarController.h"

#include "StackedPage.h"

namespace HomeCompa::Flibrary
{

class ImageViewer final : public StackedPage
{
	NON_COPY_MOVABLE(ImageViewer)

public:
	ImageViewer(
		std::shared_ptr<const IUiFactory>          uiFactory,
		std::shared_ptr<ISettings>                 settings,
		std::shared_ptr<IImageViewerController>    imageViewerController,
		std::shared_ptr<Util::ItemViewToolTipper>  itemViewToolTipper,
		std::shared_ptr<Util::ScrollBarController> scrollBarControllerNavigation,
		std::shared_ptr<Util::ScrollBarController> scrollBarControllerImages,
		QWidget*                                   parent = nullptr
	);
	~ImageViewer() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
