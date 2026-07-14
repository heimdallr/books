#include "ui_ImageViewer.h"

#include "ImageViewer.h"

#include "utilgui/GeometryRestorable.h"

using namespace HomeCompa::Flibrary;

class ImageViewer::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(
		QWidget&                                   self,
		std::shared_ptr<ISettings>                 settings,
		std::shared_ptr<IImageViewerController>    imageViewerController,
		std::shared_ptr<Util::ItemViewToolTipper>  itemViewToolTipper,
		std::shared_ptr<Util::ScrollBarController> scrollBarControllerNavigation,
		std::shared_ptr<Util::ScrollBarController> scrollBarControllerImages
	)
		: GeometryRestorable(*this, std::move(settings), "ImageViewer")
		, GeometryRestorableObserver(self)
		, m_imageViewerController { std::move(imageViewerController) }
		, m_itemViewToolTipper { std::move(itemViewToolTipper) }
		, m_scrollBarControllerNavigation { std::move(scrollBarControllerNavigation) }
		, m_scrollBarControllerImages { std::move(scrollBarControllerImages) }
	{
		m_ui.setupUi(&self);

		m_itemViewToolTipper->SetScrollArea(m_ui.navigation);
		m_scrollBarControllerNavigation->SetScrollArea(m_ui.navigation);
		m_scrollBarControllerImages->SetScrollArea(m_ui.images);

		LoadGeometry();
	}

	~Impl() override
	{
		SaveGeometry();
	}

private:
	PropagateConstPtr<IImageViewerController, std::shared_ptr>    m_imageViewerController;
	PropagateConstPtr<Util::ItemViewToolTipper, std::shared_ptr>  m_itemViewToolTipper;
	PropagateConstPtr<Util::ScrollBarController, std::shared_ptr> m_scrollBarControllerNavigation;
	PropagateConstPtr<Util::ScrollBarController, std::shared_ptr> m_scrollBarControllerImages;

	Ui::ImageViewer m_ui {};
};

ImageViewer::ImageViewer(
	std::shared_ptr<const IUiFactory>          uiFactory,
	std::shared_ptr<ISettings>                 settings,
	std::shared_ptr<IImageViewerController>    imageViewerController,
	std::shared_ptr<Util::ItemViewToolTipper>  itemViewToolTipper,
	std::shared_ptr<Util::ScrollBarController> scrollBarControllerNavigation,
	std::shared_ptr<Util::ScrollBarController> scrollBarControllerImages,
	QWidget*                                   parent
)
	: StackedPage(uiFactory->GetParentWidget(parent))
	, m_impl(*this, std::move(settings), std::move(imageViewerController), std::move(itemViewToolTipper), std::move(scrollBarControllerNavigation), std::move(scrollBarControllerImages))
{
}

ImageViewer::~ImageViewer() = default;
