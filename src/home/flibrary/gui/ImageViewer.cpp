#include "ui_ImageViewer.h"

#include "ImageViewer.h"

#include "interface/constants/SettingsConstant.h"

#include "utilgui/GeometryRestorable.h"

using namespace HomeCompa::Flibrary;

class ImageViewer::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
	, IImageViewerController::IObserver
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
		: GeometryRestorable(*this, settings, "ImageViewer")
		, GeometryRestorableObserver(self)
		, m_settings { std::move(settings) }
		, m_imageViewerController { std::move(imageViewerController) }
		, m_itemViewToolTipper { std::move(itemViewToolTipper) }
		, m_scrollBarControllerNavigation { std::move(scrollBarControllerNavigation) }
		, m_scrollBarControllerImages { std::move(scrollBarControllerImages) }
	{
		m_ui.setupUi(&self);

		m_ui.navigation->setAlternatingRowColors(m_settings->Get(Constant::Settings::PREFER_ALTERNATING_ROW_COLORS, false));

		m_itemViewToolTipper->SetScrollArea(m_ui.navigation);
		m_scrollBarControllerNavigation->SetScrollArea(m_ui.navigation);
		m_scrollBarControllerImages->SetScrollArea(m_ui.images);

		m_imageViewerController->RegisterObserver(this);

		LoadGeometry();
	}

	~Impl() override
	{
		m_imageViewerController->UnregisterObserver(this);

		SaveGeometry();
	}

private:
	void OnNavigationModelChanged(std::shared_ptr<QAbstractItemModel> model) override
	{
		m_navigationModel.reset(std::move(model));
		m_ui.navigation->setModel(m_navigationModel.get());
	}

private:
	PropagateConstPtr<ISettings, std::shared_ptr>                 m_settings;
	PropagateConstPtr<IImageViewerController, std::shared_ptr>    m_imageViewerController;
	PropagateConstPtr<Util::ItemViewToolTipper, std::shared_ptr>  m_itemViewToolTipper;
	PropagateConstPtr<Util::ScrollBarController, std::shared_ptr> m_scrollBarControllerNavigation;
	PropagateConstPtr<Util::ScrollBarController, std::shared_ptr> m_scrollBarControllerImages;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr>        m_navigationModel { std::shared_ptr<QAbstractItemModel> {} };

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
