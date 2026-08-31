#include "ui_ImageViewer.h"

#include "ImageViewer.h"

#include <QMenu>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QWidgetAction>

#include "interface/constants/ObjectConnectionID.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/localization.h"
#include "interface/logic/IMenuCustomizer.h"

#include "gutil/util.h"
#include "logic/data/DataItem.h"
#include "util/ImageUtil.h"
#include "util/ObjectsConnector.h"
#include "utilgui/GeometryRestorable.h"

#include "QtTypes.h"
#include "log.h"

using namespace HomeCompa;
using namespace HomeCompa::Flibrary;

namespace
{

constexpr auto CONTEXT       = "ImageViewer";
constexpr auto SELECT_FOLDER = QT_TRANSLATE_NOOP("ImageViewer", "Select images folder");
constexpr auto IMAGE_VIEWER  = QT_TRANSLATE_NOOP("ImageViewer", "Image Viewer");

constexpr auto ICON_SIZE              = "ui/ImageViewer/IconSize";
constexpr auto IMAGE_BACKGROUND_COLOR = "ui/ImageViewer/ImageBackgroundColor";

TR_DEF

class ImageDelegate final : public QStyledItemDelegate
{
public:
	ImageDelegate(IImageViewerController& controller, QAbstractItemView* view)
		: QStyledItemDelegate(view)
		, m_controller { controller }
		, m_view { view }
	{
	}

private: // QStyledItemDelegate
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		m_controller.PrepareImage(index);
		QStyledItemDelegate::paint(painter, option, index);
	}

	QSize sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const override
	{
		return m_view->iconSize();
	}

private:
	IImageViewerController&  m_controller;
	const QAbstractItemView* m_view;
};

} // namespace

class ImageViewer::Impl final
	: public QObject
	, Util::GeometryRestorable
	, Util::GeometryRestorableObserver
	, IImageViewerController::IObserver
	, IUiFactory::IChangeSizeWidgetObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(
		ImageViewer&                               self,
		std::shared_ptr<const IUiFactory>          uiFactory,
		std::shared_ptr<ISettings>                 settings,
		std::shared_ptr<IImageViewerController>    imageViewerController,
		std::shared_ptr<IMenuCustomizer>           menuCustomizer,
		std::shared_ptr<Util::ItemViewToolTipper>  itemViewToolTipper,
		std::shared_ptr<Util::ScrollBarController> scrollBarController,
		std::shared_ptr<ProgressBar>               progressBar
	)
		: GeometryRestorable(*this, settings, CONTEXT)
		, GeometryRestorableObserver(self)
		, m_self { self }
		, m_uiFactory { std::move(uiFactory) }
		, m_settings { std::move(settings) }
		, m_imageViewerController { std::move(imageViewerController) }
		, m_menuCustomizer { std::move(menuCustomizer) }
		, m_itemViewToolTipper { std::move(itemViewToolTipper) }
		, m_scrollBarController { std::move(scrollBarController) }
		, m_progressBar { std::move(progressBar) }
	{
		m_ui.setupUi(&self);

		self.addActions({ m_ui.actionSave, m_ui.actionChangeThumbnailSize });

		m_ui.splitter->setSizes({ 400, 100 });
		m_ui.progressLayout->addWidget(m_progressBar.get());

		auto* delegate = new ImageDelegate(*m_imageViewerController, m_ui.images);
		m_ui.images->setItemDelegate(delegate);
		m_ui.images->setModel(m_imageViewerController->GetImageModel());
		m_ui.images->setAlternatingRowColors(m_settings->Get(Constant::Settings::PREFER_ALTERNATING_ROW_COLORS, false));

		m_ui.imageScrollArea->installEventFilter(this);
		m_ui.filter->installEventFilter(this);
		m_ui.filter->addAction(m_ui.actionFilter, QLineEdit::LeadingPosition);
		m_menuCustomizer->Add(CONTEXT, self, Tr(IMAGE_VIEWER));

		m_itemViewToolTipper->SetShowForceColumns({ 0 });
		m_itemViewToolTipper->SetScrollArea(m_ui.images);
		m_scrollBarController->SetScrollArea(m_ui.images);
		m_imageViewerController->RegisterObserver(this);

		connect(m_ui.filter, &QLineEdit::textChanged, this, &Impl::OnFilterChanged);
		connect(m_ui.images->selectionModel(), &QItemSelectionModel::currentChanged, this, &Impl::OnCurrentImageChanged);
		connect(m_ui.images->selectionModel(), &QItemSelectionModel::selectionChanged, this, &Impl::OnImageSelectionChanged);
		connect(m_ui.images, &QAbstractItemView::iconSizeChanged, this, &Impl::OnIconSizeChanged);
		connect(m_ui.images, &QWidget::customContextMenuRequested, this, &Impl::OnImagesContextMenuRequested);
		connect(m_ui.image, &QWidget::customContextMenuRequested, this, &Impl::OnImageContextMenuRequested);
		connect(m_ui.actionSave, &QAction::triggered, this, &Impl::OnActionSaveTriggered);
		connect(m_ui.actionChangeThumbnailSize, &QAction::triggered, this, &Impl::OnActionChangeThumbnailSizeTriggered);
		connect(m_ui.actionSetBackgroundColor, &QAction::triggered, this, &Impl::OnActionSetBackgroundColorTriggered);

		const auto iconSize = m_settings->Get(ICON_SIZE, 256);
		m_ui.images->setIconSize(QSize(iconSize, iconSize));

		LoadGeometry();

		PLOGV << "ImageViewer created";
	}

	~Impl() override
	{
		m_imageViewerController->UnregisterObserver(this);
		SaveGeometry();

		PLOGV << "destroyed created";
	}

private: // QObject
	bool eventFilter(QObject* obj, QEvent* event) override
	{
		if (event->type() == QEvent::Type::Resize)
		{
			if (obj == m_ui.imageScrollArea)
				OnImageResized();

			if (obj == m_ui.filter)
				emit m_self.ValueGeometryChanged(Util::GetGlobalGeometry(*m_ui.filter));
		}

		return QObject::eventFilter(obj, event);
	}

private: // IImageViewerController::IObserver
	void OnImageReceived(QPixmap pixmap) override
	{
		const QString styleSheet = [&] {
			if (pixmap.isNull())
				return QString {};
			if (const auto colorName = m_settings->Get(IMAGE_BACKGROUND_COLOR, QString {}); QColor::IS_VALID_COLOR_NAME(colorName))
				return QString(Constant::Settings::BACKGROUND_COLOR_TEMPLATE).arg(colorName);
			return QString {};
		}();
		m_ui.image->setStyleSheet(styleSheet);

		m_currentImage = std::move(pixmap);
		OnImageResized();
	}

	void OnCountChanges(const int count) override
	{
		m_ui.count->setText(QString::number(count));
	}

private: // IUiFactory::IChangeSizeWidgetObserver
	void OnSizeChanged(const int size) override
	{
		m_ui.images->setIconSize(QSize(size, size));
	}

private:
	void OnCurrentImageChanged(const QModelIndex& index)
	{
		m_imageViewerController->RequestImage(index);
	}

	void OnImageSelectionChanged(const QItemSelection& selected, const QItemSelection& /*deselected*/) const
	{
		m_ui.actionSave->setEnabled(!selected.isEmpty());
	}

	void OnIconSizeChanged(const QSize& iconSize)
	{
		m_settings->Set(ICON_SIZE, iconSize.width());
		m_imageViewerController->SetImageSize(iconSize.width());
		m_ui.images->setGridSize(iconSize + QSize(4, 4));
	}

	void OnImageResized() const
	{
		auto       imageSize  = m_ui.imageScrollArea->size();
		const auto pixmapSize = m_currentImage.size();
		if (pixmapSize.width() <= imageSize.width() && pixmapSize.height() <= imageSize.height())
			return m_ui.image->setPixmap(m_currentImage);

		if (imageSize.height() * pixmapSize.width() > pixmapSize.height() * imageSize.width())
			imageSize.rheight() = pixmapSize.height() * imageSize.width() / pixmapSize.width();
		else
			imageSize.rwidth() = pixmapSize.width() * imageSize.height() / pixmapSize.height();

		m_ui.image->setPixmap(m_currentImage.scaled(imageSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	}

	void OnFilterChanged(const QString& filter)
	{
		m_imageViewerController->Filter(filter);
	}

	void OnImagesContextMenuRequested(const QPoint&)
	{
		if (QGuiApplication::keyboardModifiers() & Qt::ControlModifier)
			return OnActionChangeThumbnailSizeTriggered();

		QMenu menu(&m_self);
		menu.setFont(m_self.font());
		menu.addAction(m_ui.actionSave);
		menu.addAction(m_ui.actionChangeThumbnailSize);
		menu.exec(QCursor::pos());
	}

	void OnImageContextMenuRequested(const QPoint&) const
	{
		if (Util::HasAlpha(m_ui.image->pixmap(Qt::ReturnByValue).toImage()).pixelFormat().channelCount() < 4)
			return;

		QMenu menu(&m_self);
		menu.setFont(m_self.font());
		menu.addAction(m_ui.actionSetBackgroundColor);
		menu.exec(QCursor::pos());
	}

	void OnActionSaveTriggered()
	{
		if (const auto folder = m_uiFactory->GetExistingDirectory(CONTEXT, Tr(SELECT_FOLDER)); !folder.isEmpty())
			m_imageViewerController->Save(folder, m_ui.images->selectionModel()->selectedIndexes());
	}

	void OnActionChangeThumbnailSizeTriggered()
	{
		QMenu menu(&m_self);
		auto* menuWidget = m_uiFactory->CreateChangeSizeWidget(m_ui.images->iconSize().width(), 32, 1024, this);
		menuWidget->setFont(m_self.font());
		auto action = new QWidgetAction(&menu);
		action->setFont(m_self.font());
		action->setDefaultWidget(menuWidget);
		menu.addAction(action);
		menu.setFixedSize(menuWidget->width() + 4, menuWidget->height() + 5);
		menu.exec(QCursor::pos());
	}

	void OnActionSetBackgroundColorTriggered() const
	{
		m_uiFactory->SetBackgroundStyleSheet(*m_ui.image, IMAGE_BACKGROUND_COLOR);
	}

private:
	Ui::ImageViewer m_ui {};
	ImageViewer&    m_self;

	std::shared_ptr<const IUiFactory> m_uiFactory;

	PropagateConstPtr<ISettings, std::shared_ptr>                 m_settings;
	PropagateConstPtr<IImageViewerController, std::shared_ptr>    m_imageViewerController;
	PropagateConstPtr<IMenuCustomizer, std::shared_ptr>           m_menuCustomizer;
	PropagateConstPtr<Util::ItemViewToolTipper, std::shared_ptr>  m_itemViewToolTipper;
	PropagateConstPtr<Util::ScrollBarController, std::shared_ptr> m_scrollBarController;
	PropagateConstPtr<ProgressBar, std::shared_ptr>               m_progressBar;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr>        m_navigationModel { std::shared_ptr<QAbstractItemModel> {} };

	QPixmap m_currentImage;
};

ImageViewer::ImageViewer(
	std::shared_ptr<const IUiFactory>          uiFactory,
	std::shared_ptr<ISettings>                 settings,
	std::shared_ptr<IImageViewerController>    imageViewerController,
	std::shared_ptr<IMenuCustomizer>           menuCustomizer,
	std::shared_ptr<Util::ItemViewToolTipper>  itemViewToolTipper,
	std::shared_ptr<Util::ScrollBarController> scrollBarController,
	std::shared_ptr<ProgressBar>               progressBar,
	QWidget*                                   parent
)
	: StackedPage(*uiFactory, uiFactory->GetParentWidget(parent))
	, m_impl(
		  *this,
		  std::move(uiFactory),
		  std::move(settings),
		  std::move(imageViewerController),
		  std::move(menuCustomizer),
		  std::move(itemViewToolTipper),
		  std::move(scrollBarController),
		  std::move(progressBar)
	  )
{
	Util::ObjectsConnector::registerEmitter(ObjectConnectorID::BOOKS_SEARCH_FILTER_VALUE_GEOMETRY_CHANGED, this, SIGNAL(ValueGeometryChanged(const QRect&)));
}

ImageViewer::~ImageViewer() = default;
