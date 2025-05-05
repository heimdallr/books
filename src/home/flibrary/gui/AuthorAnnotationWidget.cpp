#include "ui_AuthorAnnotationWidget.h"

#include "AuthorAnnotationWidget.h"

#include <QAbstractTableModel>
#include <QDesktopServices>
#include <QTimer>

#include "fnd/ScopedCall.h"

#include "interface/logic/ITreeViewController.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

namespace
{

void UpdateGallerySize(QTableView& view)
{
	const auto height = view.height();

	if (height <= 0)
		return;

	view.setRowHeight(0, height);

	const auto* model = view.model();
	for (int i = 0, sz = model->columnCount(); i < sz; ++i)
		view.setColumnWidth(i, model->index(0, i).data(Qt::SizeHintRole).value<QSize>().width());
}

struct GalleryHeightHandler final : QObject
{
	QTableView* view;

	explicit GalleryHeightHandler(QTableView* parent = nullptr)
		: QObject(parent)
		, view(parent)
	{
	}

	bool eventFilter(QObject* object, QEvent* event) override
	{
		if (event->type() == QEvent::Resize)
			UpdateGallerySize(*view);

		return QObject::eventFilter(object, event);
	}
};

} // namespace

class AuthorAnnotationWidget::Impl final
	: IAuthorAnnotationController::IObserver
	, QAbstractTableModel
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(QFrame* self,
	     std::shared_ptr<IAuthorAnnotationController> annotationController,
	     std::shared_ptr<ScrollBarController> scrollBarControllerText,
	     std::shared_ptr<ScrollBarController> scrollBarControllerImages)
		: m_self { self }
		, m_annotationController { std::move(annotationController) }
		, m_scrollBarControllerText { std::move(scrollBarControllerText) }
		, m_scrollBarControllerImages { std::move(scrollBarControllerImages) }
	{
		m_ui.setupUi(m_self);

		m_ui.gallery->setVisible(false);
		m_ui.gallery->setModel(this);
		m_ui.gallery->installEventFilter(new GalleryHeightHandler(m_ui.gallery));

		m_ui.info->installEventFilter(m_scrollBarControllerText.get());
		m_scrollBarControllerText->SetScrollArea(m_ui.scrollArea);

		m_ui.gallery->installEventFilter(m_scrollBarControllerImages.get());
		m_scrollBarControllerImages->SetScrollArea(m_ui.gallery);

		m_annotationController->RegisterObserver(this);

		connect(m_ui.info, &QLabel::linkActivated, m_ui.info, [&](const QString& link) { QDesktopServices::openUrl(link); });
		connect(m_ui.info, &QLabel::linkHovered, m_ui.info, [&](const QString& link) { PLOGI_IF(!link.isEmpty()) << link; });
	}

	~Impl() override
	{
		m_annotationController->UnregisterObserver(this);
	}

	void Show(const bool value)
	{
		m_show = value;
		SetVisible();
	}

	void OnResize() const
	{
		const auto width = m_ui.scrollArea->viewport()->width();
		m_ui.info->setMinimumWidth(width);
		m_ui.info->setMaximumWidth(width);
	}

private: // IAuthorAnnotationController::IObserver
	void OnReadyChanged() override
	{
		SetVisible();
	}

	void OnAuthorChanged(const QString& text, const std::vector<QByteArray>& images) override
	{
		m_ui.info->setText(text);

		const ScopedCall resetGuard([this] { beginResetModel(); },
		                            [this]
		                            {
										endResetModel();
										UpdateGallerySize(*m_ui.gallery);
										m_ui.gallery->setVisible(!m_images.empty());
									});
		m_images.clear();
		std::ranges::transform(images,
		                       std::back_inserter(m_images),
		                       [](const auto& item)
		                       {
								   QPixmap pixmap;
								   pixmap.loadFromData(item);
								   return pixmap;
							   });
	}

private: // QAbstractItemModel
	int rowCount(const QModelIndex& parent = QModelIndex()) const override
	{
		return parent.isValid() ? 0 : 1;
	}

	int columnCount(const QModelIndex& parent = QModelIndex()) const override
	{
		return parent.isValid() ? 0 : static_cast<int>(m_images.size());
	}

	QVariant data(const QModelIndex& index, const int role) const override
	{
		assert(index.column() < columnCount());

		auto pixmap = m_images[index.column()];
		const auto height = m_ui.gallery->height();

		switch (role)
		{
			case Qt::SizeHintRole:
				return QSize { height * pixmap.width() / pixmap.height(), height };

			case Qt::DecorationRole:
				return m_images[index.column()].scaled(height * pixmap.width() / pixmap.height(), height);

			default:
				break;
		}

		return {};
	}

private:
	void SetVisible() const
	{
		QTimer::singleShot(0, [this] { m_self->parentWidget()->setVisible(m_show && m_annotationController->IsReady()); });
	}

private:
	QFrame* m_self;
	PropagateConstPtr<IAuthorAnnotationController, std::shared_ptr> m_annotationController;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> m_scrollBarControllerText;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> m_scrollBarControllerImages;
	bool m_show { true };
	std::vector<QPixmap> m_images;
	Ui::AuthorAnnotationWidget m_ui {};
};

AuthorAnnotationWidget::AuthorAnnotationWidget(std::shared_ptr<IAuthorAnnotationController> annotationController,
                                               std::shared_ptr<ScrollBarController> scrollBarControllerText,
                                               std::shared_ptr<ScrollBarController> scrollBarControllerImages,
                                               QWidget* parent)
	: QFrame(parent)
	, m_impl(this, std::move(annotationController), std::move(scrollBarControllerText), std::move(scrollBarControllerImages))
{
}

AuthorAnnotationWidget::~AuthorAnnotationWidget() = default;

void AuthorAnnotationWidget::Show(const bool value)
{
	m_impl->Show(value);
}

void AuthorAnnotationWidget::resizeEvent(QResizeEvent*)
{
	m_impl->OnResize();
}
