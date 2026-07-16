#include "ui_ImageViewer.h"

#include "ImageViewer.h"

#include <QStyledItemDelegate>

#include "interface/constants/ImageModelRole.h"

#include "utilgui/GeometryRestorable.h"

using namespace HomeCompa::Flibrary;

namespace
{

constexpr auto ICON_SIZE = "ui/ImageViewer/IconSize";

class ImageDelegate final : public QStyledItemDelegate
{
public:
	explicit ImageDelegate(QAbstractItemView* view = nullptr)
		: QStyledItemDelegate(view)
		, m_view { view }
	{
	}

private: // QStyledItemDelegate
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		if (!index.data(ImageModelRole::Ready).toBool())
			m_view->model()->setData(index, {}, ImageModelRole::Prepare);

		QStyledItemDelegate::paint(painter, option, index);
	}

	QSize sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const override
	{
		return m_view->iconSize();
	}

private:
	const QAbstractItemView* m_view;
};

} // namespace

class ImageViewer::Impl final
	: public QObject
	, Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(
		QWidget&                                   self,
		std::shared_ptr<ISettings>                 settings,
		std::shared_ptr<IImageViewerController>    imageViewerController,
		std::shared_ptr<Util::ItemViewToolTipper>  itemViewToolTipper,
		std::shared_ptr<Util::ScrollBarController> scrollBarController
	)
		: GeometryRestorable(*this, settings, "ImageViewer")
		, GeometryRestorableObserver(self)
		, m_settings { std::move(settings) }
		, m_imageViewerController { std::move(imageViewerController) }
		, m_itemViewToolTipper { std::move(itemViewToolTipper) }
		, m_scrollBarController { std::move(scrollBarController) }
	{
		m_ui.setupUi(&self);

		auto* delegate = new ImageDelegate(m_ui.images);
		m_ui.images->setItemDelegate(delegate);
		m_ui.images->setModel(m_imageViewerController->GetImageModel());

		m_itemViewToolTipper->SetShowForceColumns({ 0 });
		m_itemViewToolTipper->SetScrollArea(m_ui.images);
		m_scrollBarController->SetScrollArea(m_ui.images);

		connect(m_ui.images, &QAbstractItemView::iconSizeChanged, this, &Impl::OnIconSizeChanged);
		const auto iconSize = m_settings->Get(ICON_SIZE, 256);
		m_ui.images->setIconSize(QSize(iconSize, iconSize));

		LoadGeometry();
	}

	~Impl() override
	{
		SaveGeometry();
	}

private:
	void OnIconSizeChanged(const QSize& iconSize)
	{
		m_settings->Set(ICON_SIZE, iconSize.width());
		m_imageViewerController->SetImageSize(iconSize.width());
		m_ui.images->setGridSize(iconSize + QSize(4, 4));
	}

private:
	PropagateConstPtr<ISettings, std::shared_ptr>                 m_settings;
	PropagateConstPtr<IImageViewerController, std::shared_ptr>    m_imageViewerController;
	PropagateConstPtr<Util::ItemViewToolTipper, std::shared_ptr>  m_itemViewToolTipper;
	PropagateConstPtr<Util::ScrollBarController, std::shared_ptr> m_scrollBarController;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr>        m_navigationModel { std::shared_ptr<QAbstractItemModel> {} };

	Ui::ImageViewer m_ui {};
};

ImageViewer::ImageViewer(
	std::shared_ptr<const IUiFactory>          uiFactory,
	std::shared_ptr<ISettings>                 settings,
	std::shared_ptr<IImageViewerController>    imageViewerController,
	std::shared_ptr<Util::ItemViewToolTipper>  itemViewToolTipper,
	std::shared_ptr<Util::ScrollBarController> scrollBarController,
	QWidget*                                   parent
)
	: StackedPage(*uiFactory, uiFactory->GetParentWidget(parent))
	, m_impl(*this, std::move(settings), std::move(imageViewerController), std::move(itemViewToolTipper), std::move(scrollBarController))
{
}

ImageViewer::~ImageViewer() = default;
