#include "ui_AnnotationWidget.h"

#include "AnnotationWidget.h"

#include <QBuffer>
#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QPainter>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IDataItem.h"

#include "logic/TreeViewController/AbstractTreeViewController.h"
#include "logic/data/DataItem.h"
#include "logic/model/IModelObserver.h"
#include "util/FunctorExecutionForwarder.h"
#include "util/IExecutor.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT = "Annotation";
constexpr auto SELECT_IMAGE_FILE_NAME = QT_TRANSLATE_NOOP("Annotation", "Select image file name");
constexpr auto SELECT_IMAGE_FOLDER = QT_TRANSLATE_NOOP("Annotation", "Select images folder");
constexpr auto IMAGE_FILE_NAME_FILTER = QT_TRANSLATE_NOOP("Annotation", "Jpeg images (*.jpg *.jpeg);;PNG images (*.png);;All files (*.*)");
constexpr auto SAVE_ALL_PICS_ACTION_TEXT = QT_TRANSLATE_NOOP("Annotation", "Save &all images (%1)...");
constexpr auto SAVED_ALL = QT_TRANSLATE_NOOP("Annotation", "All %1 images were successfully saved");
constexpr auto SAVED_PARTIALLY = QT_TRANSLATE_NOOP("Annotation", "%1 out of %2 images were saved");
constexpr auto SAVED_WITH_ERRORS = QT_TRANSLATE_NOOP("Annotation", "%1 images out of %2 could not be saved");
constexpr auto CANNOT_SAVE_IMAGE = QT_TRANSLATE_NOOP("Annotation", "Cannot save image to %1");
constexpr auto CANNOT_OPEN_IMAGE = QT_TRANSLATE_NOOP("Annotation", "Cannot open %1");

#if (false) // NOLINT(readability-avoid-unconditional-preprocessor-if)
QT_TRANSLATE_NOOP("Annotation", "Read")
QT_TRANSLATE_NOOP("Annotation", "AsIs")
QT_TRANSLATE_NOOP("Annotation", "Archive")
QT_TRANSLATE_NOOP("Annotation", "Script")
QT_TRANSLATE_NOOP("Annotation", "Inpx")
#endif

constexpr auto DIALOG_KEY = "Image";

constexpr const char* CUSTOM_URL_SCHEMA[] { Loc::AUTHORS, Loc::SERIES, Loc::GENRES, Loc::KEYWORDS, Loc::UPDATES, Loc::ARCHIVE, Loc::LANGUAGE, Loc::GROUPS, nullptr };

TR_DEF

bool SaveImage(QString fileName, const QByteArray& bytes)
{
	if (const QFileInfo fileInfo(fileName); fileInfo.suffix().isEmpty())
		fileName += ".jpg";

	if (QFile::exists(fileName))
		PLOGW << fileName << " already exists and will be overwritten";

	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly))
	{
		PLOGE << "Cannot open to write into " << fileName;
		return false;
	}

	if (file.write(bytes) != bytes.size())
	{
		PLOGW << "Write incomplete into " << fileName;
		return false;
	}

	return true;
}

struct CoverButtonType
{
	enum
	{
		Previous,
		Home,
		Next,
	};
};

} // namespace

class AnnotationWidget::Impl final
	: QObject
	, IAnnotationController::IObserver
	, IAnnotationController::IStrategy
	, IModelObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(AnnotationWidget& self,
	     const std::shared_ptr<const IModelProvider>& modelProvider,
	     const std::shared_ptr<const ILogicFactory>& logicFactory,
	     const std::shared_ptr<ICollectionController>& collectionController,
	     std::shared_ptr<const IReaderController> readerController,
	     std::shared_ptr<ISettings> settings,
	     std::shared_ptr<IAnnotationController> annotationController,
	     std::shared_ptr<IUiFactory> uiFactory,
	     std::shared_ptr<IBooksExtractorProgressController> progressController,
	     std::shared_ptr<ItemViewToolTipper> itemViewToolTipperContent,
	     std::shared_ptr<ScrollBarController> scrollBarControllerContent,
	     std::shared_ptr<ScrollBarController> scrollBarControllerAnnotation)
		: m_self { self }
		, m_readerController { std::move(readerController) }
		, m_settings { std::move(settings) }
		, m_annotationController { std::move(annotationController) }
		, m_modelProvider { modelProvider }
		, m_logicFactory { logicFactory }
		, m_uiFactory { std::move(uiFactory) }
		, m_progressController { std::move(progressController) }
		, m_itemViewToolTipperContent { std::move(itemViewToolTipperContent) }
		, m_scrollBarControllerContent { std::move(scrollBarControllerContent) }
		, m_scrollBarControllerAnnotation { std::move(scrollBarControllerAnnotation) }
		, m_navigationController { logicFactory->GetTreeViewController(ItemType::Navigation) }
		, m_currentCollectionId { collectionController->GetActiveCollectionId() }
	{
		m_ui.setupUi(&m_self);

		m_ui.mainWidget->installEventFilter(this);
		m_ui.content->viewport()->installEventFilter(m_itemViewToolTipperContent.get());
		m_ui.content->viewport()->installEventFilter(m_scrollBarControllerContent.get());
		m_scrollBarControllerContent->SetScrollArea(m_ui.content);

		m_ui.info->installEventFilter(m_scrollBarControllerAnnotation.get());
		m_scrollBarControllerAnnotation->SetScrollArea(m_ui.scrollArea);

		const auto setCustomPalette = [](QWidget& widget)
		{
			auto palette = widget.palette();
			palette.setColor(QPalette::ColorRole::Window, palette.color(QPalette::ColorRole::Base));
			widget.setPalette(palette);
		};

		m_ui.coverArea->setVisible(false);
		setCustomPalette(*m_ui.coverArea);
		setCustomPalette(*m_ui.scrollArea->viewport());

		m_progressTimer.setSingleShot(true);
		m_progressTimer.setInterval(std::chrono::milliseconds(300));

		m_ui.content->header()->setDefaultAlignment(Qt::AlignCenter);

		m_annotationController->RegisterObserver(this);

		connect(m_ui.info, &QLabel::linkActivated, m_ui.info, [&](const QString& link) { OnLinkActivated(link); });

		const auto onCoverClicked = [&](const QPoint& pos)
		{
			if (m_covers.size() < 2)
				return;

			switch (3 * pos.x() / m_ui.cover->width())
			{
				case 0:
					if (*m_currentCoverIndex == 0)
						m_currentCoverIndex = m_covers.size() - 1;
					else
						--*m_currentCoverIndex;
					break;

				case 1:
					m_currentCoverIndex = m_coverIndex;
					m_coverButtons[CoverButtonType::Home]->setVisible(false);
					break;

				case 2:
					if (++*m_currentCoverIndex >= m_covers.size())
						m_currentCoverIndex = 0;
					break;

				default:
					assert(false && "wtf?");
			}

			OnResize();
		};
		connect(m_ui.cover, &ClickableLabel::clicked, onCoverClicked);

		m_ui.cover->addAction(m_ui.actionOpenImage);
		m_ui.cover->addAction(m_ui.actionCopyImage);
		m_ui.cover->addAction(m_ui.actionSaveImageAs);
		m_ui.cover->addAction(m_ui.actionSaveAllImages);

		const auto openImage = [this]
		{
			assert(!m_covers.empty());
			const auto& [name, bytes] = m_covers[*m_currentCoverIndex];
			auto path = m_logicFactory.lock()->CreateTemporaryDir()->filePath(name);
			if (const QFileInfo fileInfo(path); fileInfo.suffix().isEmpty())
				path += ".jpg";

			if (!SaveImage(path, bytes))
				return m_uiFactory->ShowError(Tr(CANNOT_SAVE_IMAGE).arg(path));
			if (!QDesktopServices::openUrl(path))
				m_uiFactory->ShowError(Tr(CANNOT_OPEN_IMAGE).arg(path));
		};
		connect(m_ui.cover, &ClickableLabel::doubleClicked, &m_self, openImage);

		connect(m_ui.cover, &ClickableLabel::mouseEnter, &m_self, [this] { OnCoverEnter(); });
		connect(m_ui.cover, &ClickableLabel::mouseLeave, &m_self, [this] { OnCoverLeave(); });

		connect(m_ui.actionOpenImage, &QAction::triggered, &m_self, openImage);

		connect(m_ui.actionCopyImage,
		        &QAction::triggered,
		        &m_self,
		        [this]
		        {
					assert(!m_covers.empty());

					QPixmap pixmap;
					[[maybe_unused]] const auto ok = pixmap.loadFromData(m_covers[*m_currentCoverIndex].bytes);
					QGuiApplication::clipboard()->setImage(pixmap.toImage());
				});

		connect(m_ui.actionSaveImageAs,
		        &QAction::triggered,
		        &m_self,
		        [this]
		        {
					assert(!m_covers.empty());
					if (const auto fileName = m_uiFactory->GetSaveFileName(DIALOG_KEY, Tr(SELECT_IMAGE_FILE_NAME), IMAGE_FILE_NAME_FILTER); !fileName.isEmpty())
						SaveImage(fileName, m_covers[*m_currentCoverIndex].bytes);
				});

		connect(m_ui.actionSaveAllImages,
		        &QAction::triggered,
		        &m_self,
		        [this]
		        {
					auto folder = m_uiFactory->GetExistingDirectory(DIALOG_KEY, Tr(SELECT_IMAGE_FOLDER));
					if (folder.isEmpty())
						return;

					std::shared_ptr progressItem = m_progressController->Add(static_cast<int>(m_covers.size()));
					std::shared_ptr executor = ILogicFactory::Lock(m_logicFactory)->GetExecutor();

					(*executor)({ "Save images",
			                      [this, executor, folder = std::move(folder), covers = m_covers, progressItem = std::move(progressItem)]() mutable
			                      {
									  size_t savedCount = 0;
									  for (const auto& [name, bytes] : covers)
									  {
										  if (SaveImage(QString("%1/%2").arg(folder).arg(name), bytes))
											  ++savedCount;

										  progressItem->Increment(1);
										  if (progressItem->IsStopped())
											  break;
									  }

									  return [this, executor = std::move(executor), progressItem = std::move(progressItem), savedCount, totalCount = covers.size()](size_t) mutable
									  {
										  if (savedCount == totalCount)
											  m_uiFactory->ShowInfo(Tr(SAVED_ALL).arg(savedCount));
										  else if (progressItem->IsStopped())
											  m_uiFactory->ShowInfo(Tr(SAVED_PARTIALLY).arg(savedCount).arg(totalCount));
										  else
											  m_uiFactory->ShowWarning(Tr(SAVED_WITH_ERRORS).arg(totalCount - savedCount).arg(totalCount));

										  progressItem.reset();
										  executor.reset();
									  };
								  } });
				});

		const auto createCoverButton = [this, onCoverClicked](const QString& iconFileName)
		{
			auto* btn = new QToolButton(m_ui.cover);
			btn->setVisible(false);
			btn->setIcon(QIcon(iconFileName));
			connect(btn, &QAbstractButton::clicked, &m_self, [this, onCoverClicked] { onCoverClicked(m_ui.cover->mapFromGlobal(QCursor::pos())); });
			m_coverButtons.push_back(btn);
		};

		createCoverButton(":/icons/left.svg");
		createCoverButton(":/icons/center.svg");
		createCoverButton(":/icons/right.svg");
	}

	~Impl() override
	{
		m_annotationController->UnregisterObserver(this);
	}

	void ShowContent(const bool value)
	{
		m_showContent = value;
		m_ui.contentWidget->setVisible(value && m_ui.content->model());
	}

	void ShowCover(const bool value)
	{
		m_showCover = value;
		OnResize();
	}

	void ShowCoverButtons(const bool value)
	{
		m_coverButtonsVisible = value;
	}

	void OnResize()
	{
		m_ui.coverArea->setVisible(m_showCover);
		m_coverButtonsEnabled = false;

		if (m_covers.empty() || !m_ui.coverArea->isVisible())
		{
			m_ui.coverArea->setMinimumWidth(0);
			m_ui.coverArea->setMaximumWidth(0);
			return;
		}

		auto imgHeight = m_ui.mainWidget->height();
		auto imgWidth = m_ui.mainWidget->width() / 3;

		if (QPixmap pixmap; pixmap.loadFromData(m_covers[*m_currentCoverIndex].bytes))
		{
			if (imgHeight * pixmap.width() > pixmap.height() * imgWidth)
				imgHeight = pixmap.height() * imgWidth / pixmap.width();
			else
				imgWidth = pixmap.width() * imgHeight / pixmap.height();

			m_ui.cover->setPixmap(pixmap.scaled(imgWidth, imgHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		}
		else
		{
			const QIcon icon(":/icons/unsupported-image.svg");
			const auto defaultSize = icon.availableSizes().front();
			imgWidth = imgHeight * defaultSize.width() / defaultSize.height();
			m_ui.cover->setPixmap(icon.pixmap(imgWidth, imgHeight));
		}

		if (m_covers.size() > 1)
			m_coverButtonsEnabled = true;

		m_ui.coverArea->setMinimumWidth(imgWidth);
		m_ui.coverArea->setMaximumWidth(imgWidth);

		const QFontMetrics metrics(m_self.font());
		const auto height = 3 * metrics.lineSpacing() / 2;
		const QSize size { height, height };
		const auto top = imgHeight - height - height / 8;
		m_coverButtons[CoverButtonType::Previous]->setGeometry(QRect {
			QPoint { height / 8, top },
			size
        });
		m_coverButtons[CoverButtonType::Next]->setGeometry(QRect {
			QPoint { imgWidth - height - height / 8, top },
			size
        });
		m_coverButtons[CoverButtonType::Home]->setGeometry(QRect {
			QPoint { (imgWidth - height) / 2, top },
			size
        });
	}

private: // QObject
	bool eventFilter(QObject* obj, QEvent* event) override
	{
		if (event->type() == QEvent::Type::Resize)
			OnResize();

		return QObject::eventFilter(obj, event);
	}

private: // IAnnotationController::IObserver
	void OnAnnotationRequested() override
	{
		m_ui.contentWidget->setVisible(false);
		m_ui.content->setModel(nullptr);
		m_ui.coverArea->setVisible(false);
		m_ui.cover->setPixmap({});
		m_ui.cover->setCursor(Qt::ArrowCursor);
		m_ui.info->setText({});
		m_covers.clear();
		m_currentCoverIndex.reset();
		m_coverIndex.reset();
	}

	void OnAnnotationChanged(const IAnnotationController::IDataProvider& dataProvider) override
	{
		auto annotation = m_annotationController->CreateAnnotation(dataProvider, *this);

		m_ui.info->setText(annotation);
		m_covers = dataProvider.GetCovers();
		m_ui.actionSaveAllImages->setText(Tr(SAVE_ALL_PICS_ACTION_TEXT).arg(m_covers.size()));
		if (m_covers.empty())
			return;

		if (m_coverIndex = dataProvider.GetCoverIndex(); !m_coverIndex)
			m_coverIndex = 0;
		m_currentCoverIndex = m_coverIndex;

		if (m_covers.size() > 1)
			m_ui.cover->setCursor(Qt::PointingHandCursor);

		if (dataProvider.GetContent()->GetChildCount() > 0)
		{
			if (m_showContent)
				m_ui.contentWidget->setVisible(true);
			m_contentModel.reset(IModelProvider::Lock(m_modelProvider)->CreateTreeModel(dataProvider.GetContent(), *this, true));
			m_ui.content->setModel(m_contentModel.get());
		}

		OnResize();
	}

	void OnJokeChanged(const QString& value) override
	{
		m_ui.info->setText(value);
	}

	void OnArchiveParserProgress(const int percents) override
	{
		m_forwarder.Forward(
			[&, percents]
			{
				if (percents)
				{
					if (!m_progressTimer.isActive())
						m_ui.info->setText(percents == 100 ? QString {} : QString("%1%").arg(percents));
				}
				else
				{
					m_progressTimer.start();
				}
			});
	}

private: // IAnnotationController::IUrlGenerator
	QString GenerateUrl(const char* type, const QString& id, const QString& str) const override
	{
		return str.isEmpty() ? QString {} : QString("<a href=%1//%2>%3</a>").arg(type, id, str);
	}

	QString GenerateStars(const int rate) const override
	{
		return QString { rate, QChar(m_starSymbol) };
	}

private:
	void OnLinkActivated(const QString& link)
	{
		const auto url = link.split("://");
		assert(url.size() == 2);
		if (QString(Constant::BOOK).startsWith(url.front()))
		{
			return m_readerController->Read(url.back().toLongLong());
		}
		if (std::ranges::none_of(CUSTOM_URL_SCHEMA, [&](const char* schema) { return QString(schema).startsWith(url.front()); }))
		{
			QDesktopServices::openUrl(link);
			return;
		}

		m_settings->Set(QString(Constant::Settings::RECENT_NAVIGATION_ID_KEY).arg(m_currentCollectionId).arg(url.front()), url.back());
		m_navigationController->SetMode(url.front());
	}

	void OnCoverEnter() const
	{
		if (!m_coverButtonsVisible || !m_coverButtonsEnabled || m_ui.cover->size().width() < m_coverButtons.front()->size().width() * 4)
			return;

		for (auto* btn : m_coverButtons)
			btn->setVisible(true);

		m_coverButtons[CoverButtonType::Home]->setVisible(m_currentCoverIndex != m_coverIndex);
	}

	void OnCoverLeave() const
	{
		for (auto* btn : m_coverButtons)
			btn->setVisible(false);
	}

private:
	AnnotationWidget& m_self;
	std::shared_ptr<const IReaderController> m_readerController;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<IAnnotationController, std::shared_ptr> m_annotationController;
	std::weak_ptr<const IModelProvider> m_modelProvider;
	std::weak_ptr<const ILogicFactory> m_logicFactory;
	PropagateConstPtr<IUiFactory, std::shared_ptr> m_uiFactory;
	PropagateConstPtr<IBooksExtractorProgressController, std::shared_ptr> m_progressController;
	PropagateConstPtr<ItemViewToolTipper, std::shared_ptr> m_itemViewToolTipperContent;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> m_scrollBarControllerContent;
	PropagateConstPtr<ScrollBarController, std::shared_ptr> m_scrollBarControllerAnnotation;

	PropagateConstPtr<ITreeViewController, std::shared_ptr> m_navigationController;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_contentModel { std::shared_ptr<QAbstractItemModel> {} };
	Ui::AnnotationWidget m_ui {};
	IAnnotationController::IDataProvider::Covers m_covers;
	const QString m_currentCollectionId;
	std::optional<size_t> m_coverIndex;
	std::optional<size_t> m_currentCoverIndex;
	bool m_showContent { true };
	bool m_showCover { true };
	Util::FunctorExecutionForwarder m_forwarder;
	QTimer m_progressTimer;

	std::vector<QAbstractButton*> m_coverButtons;
	bool m_coverButtonsEnabled { false };
	bool m_coverButtonsVisible { true };
	const int m_starSymbol { m_settings->Get(Constant::Settings::STAR_SYMBOL_KEY, Constant::Settings::STAR_SYMBOL_DEFAULT) };
};

AnnotationWidget::AnnotationWidget(const std::shared_ptr<const IModelProvider>& modelProvider,
                                   const std::shared_ptr<const ILogicFactory>& logicFactory,
                                   const std::shared_ptr<ICollectionController>& collectionController,
                                   std::shared_ptr<const IReaderController> readerController,
                                   std::shared_ptr<ISettings> settings,
                                   std::shared_ptr<IAnnotationController> annotationController,
                                   std::shared_ptr<IUiFactory> uiFactory,
                                   std::shared_ptr<IBooksExtractorProgressController> progressController,
                                   std::shared_ptr<ItemViewToolTipper> itemViewToolTipperContent,
                                   std::shared_ptr<ScrollBarController> scrollBarControllerContent,
                                   std::shared_ptr<ScrollBarController> scrollBarControllerAnnotation,
                                   QWidget* parent)
	: QWidget(parent)
	, m_impl(*this,
             modelProvider,
             logicFactory,
             collectionController,
             std::move(readerController),
             std::move(settings),
             std::move(annotationController),
             std::move(uiFactory),
             std::move(progressController),
             std::move(itemViewToolTipperContent),
             std::move(scrollBarControllerContent),
             std::move(scrollBarControllerAnnotation))
{
	PLOGV << "AnnotationWidget created";
}

AnnotationWidget::~AnnotationWidget()
{
	PLOGV << "AnnotationWidget destroyed";
}

void AnnotationWidget::ShowContent(const bool value)
{
	m_impl->ShowContent(value);
}

void AnnotationWidget::ShowCover(const bool value)
{
	m_impl->ShowCover(value);
}

void AnnotationWidget::ShowCoverButtons(const bool value)
{
	m_impl->ShowCoverButtons(value);
}
