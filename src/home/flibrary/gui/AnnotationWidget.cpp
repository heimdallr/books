#include "ui_AnnotationWidget.h"
#include "AnnotationWidget.h"

#include <QBuffer>
#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QPainter>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QTimer>

#include <plog/Log.h>

#include "GuiUtil/GeometryRestorable.h"
#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/IDataItem.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IModelProvider.h"
#include "interface/logic/IProgressController.h"
#include "interface/ui/IRateStarsProvider.h"
#include "interface/ui/IUiFactory.h"
#include "logic/data/DataItem.h"
#include "logic/model/IModelObserver.h"
#include "logic/TreeViewController/AbstractTreeViewController.h"
#include "util/FunctorExecutionForwarder.h"
#include "util/IExecutor.h"
#include "util/ISettings.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

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

#if(false)
QT_TRANSLATE_NOOP("Annotation", "Read")
QT_TRANSLATE_NOOP("Annotation", "AsIs")
QT_TRANSLATE_NOOP("Annotation", "Archive")
QT_TRANSLATE_NOOP("Annotation", "Script")
QT_TRANSLATE_NOOP("Annotation", "Inpx")
#endif

constexpr auto SPLITTER_KEY = "ui/Annotation/Splitter";
constexpr auto DIALOG_KEY = "Image";

constexpr const char * CUSTOM_URL_SCHEMA[]
{
	Loc::AUTHORS,
	Loc::SERIES,
	Loc::GENRES,
	Loc::KEYWORDS,
	Loc::ARCHIVE,
	Loc::GROUPS,
	nullptr
};
static_assert(static_cast<size_t>(NavigationMode::Last) == std::size(CUSTOM_URL_SCHEMA));

TR_DEF

bool SaveImage(QString fileName, const QByteArray & bytes)
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

}

class AnnotationWidget::Impl final
	: QObject
	, IAnnotationController::IObserver
	, IModelObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(AnnotationWidget & self
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<IAnnotationController> annotationController
		, const std::shared_ptr<const IModelProvider>& modelProvider
		, const std::shared_ptr<const ILogicFactory>& logicFactory
		, std::shared_ptr<IUiFactory> uiFactory
		, std::shared_ptr<IBooksExtractorProgressController> progressController
		, const std::shared_ptr<ICollectionController> & collectionController
		, std::shared_ptr<const IRateStarsProvider> rateStarsProvider
	)
		: m_self(self)
		, m_settings(std::move(settings))
		, m_annotationController(std::move(annotationController))
		, m_modelProvider(modelProvider)
		, m_logicFactory(logicFactory)
		, m_uiFactory(std::move(uiFactory))
		, m_progressController(std::move(progressController))
		, m_rateStarsProvider(std::move(rateStarsProvider))
		, m_navigationController(logicFactory->GetTreeViewController(ItemType::Navigation))
		, m_currentCollectionId(collectionController->GetActiveCollectionId())
	{
		m_ui.setupUi(&m_self);

		m_ui.mainWidget->installEventFilter(this);

		const auto setCustomPalette = [] (QWidget& widget)
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

		if (const auto value = m_settings->Get(SPLITTER_KEY); value.isValid())
			m_ui.splitter->restoreState(value.toByteArray());
		else
			Util::InitSplitter(m_ui.splitter);

		m_ui.content->header()->setDefaultAlignment(Qt::AlignCenter);

		m_annotationController->RegisterObserver(this);

		connect(m_ui.info, &QLabel::linkActivated, m_ui.info, [&] (const QString & link) { OnLinkActivated(link); });
		connect(m_ui.cover, &ClickableLabel::clicked, [&] (const QPoint & pos)
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
					break;

				case 2:
					if (++*m_currentCoverIndex >= m_covers.size())
						m_currentCoverIndex = 0;
					break;

				default:
					assert(false && "wtf?");
			}

			OnResize();
		});

		m_ui.cover->addAction(m_ui.actionOpenImage);
		m_ui.cover->addAction(m_ui.actionCopyImage);
		m_ui.cover->addAction(m_ui.actionSaveImageAs);
		m_ui.cover->addAction(m_ui.actionSaveAllImages);

		const auto openImage = [this]
		{
			assert(!m_covers.empty());
			const auto & [name, bytes] = m_covers[*m_currentCoverIndex];
			auto path = m_logicFactory.lock()->CreateTemporaryDir()->filePath(name);
			if (const QFileInfo fileInfo(path); fileInfo.suffix().isEmpty())
				path += ".jpg";

			if (!SaveImage(path, bytes))
				return m_uiFactory->ShowError(Tr(CANNOT_SAVE_IMAGE).arg(path));
			if (!QDesktopServices::openUrl(path))
				m_uiFactory->ShowError(Tr(CANNOT_OPEN_IMAGE).arg(path));
		};

		connect(m_ui.cover, &ClickableLabel::doubleClicked, &m_self, openImage);

		connect(m_ui.actionOpenImage, &QAction::triggered, &m_self, openImage);

		connect(m_ui.actionCopyImage, &QAction::triggered, &m_self, [this]
		{
			assert(!m_covers.empty());

			QPixmap pixmap;
			[[maybe_unused]] const auto ok = pixmap.loadFromData(m_covers[*m_currentCoverIndex].bytes);
			QGuiApplication::clipboard()->setImage(pixmap.toImage());
		});

		connect(m_ui.actionSaveImageAs, &QAction::triggered, &m_self, [this]
		{
			assert(!m_covers.empty());
			if (const auto fileName = m_uiFactory->GetSaveFileName(DIALOG_KEY, Tr(SELECT_IMAGE_FILE_NAME), IMAGE_FILE_NAME_FILTER); !fileName.isEmpty())
				SaveImage(fileName, m_covers[*m_currentCoverIndex].bytes);
		});

		connect(m_ui.actionSaveAllImages, &QAction::triggered, &m_self, [this]
		{
			auto folder = m_uiFactory->GetExistingDirectory(DIALOG_KEY, Tr(SELECT_IMAGE_FOLDER));
			if (folder.isEmpty())
				return;

			std::shared_ptr progressItem = m_progressController->Add(static_cast<int>(m_covers.size()));
			std::shared_ptr executor = ILogicFactory::Lock(m_logicFactory)->GetExecutor();

			(*executor)({ "Save images", [this, executor, folder = std::move(folder), covers = m_covers, progressItem = std::move(progressItem)] () mutable
			{
				size_t savedCount = 0;
				for (const auto & [name, bytes] : covers)
				{
					if (SaveImage(QString("%1/%2").arg(folder).arg(name), bytes))
						++savedCount;

					progressItem->Increment(1);
					if (progressItem->IsStopped())
						break;
				}

				return [this, executor = std::move(executor), progressItem = std::move(progressItem), savedCount, totalCount = covers.size()] (size_t) mutable
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
	}

	~Impl() override
	{
		m_settings->Set(SPLITTER_KEY, m_ui.splitter->saveState());
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

	void OnResize()
	{
		m_ui.coverArea->setVisible(m_showCover);

		if (m_covers.empty() || !m_ui.coverArea->isVisible())
		{
			m_ui.coverArea->setMinimumWidth(0);
			m_ui.coverArea->setMaximumWidth(0);
			return;
		}

		auto imgHeight = m_ui.mainWidget->height();
		auto imgWidth = m_ui.mainWidget->width() / 3;

		QPixmap pixmap;
		if (!pixmap.loadFromData(m_covers[*m_currentCoverIndex].bytes))
		{
			const auto defaultSize = m_svgRenderer.defaultSize();
			imgWidth = imgHeight * defaultSize.width() / defaultSize.height();
			pixmap = QPixmap(imgWidth, imgHeight);

			pixmap.fill(Qt::transparent);
			QPainter p(&pixmap);
			m_svgRenderer.render(&p, QRect(QPoint {}, pixmap.size()));

			m_ui.cover->setPixmap(pixmap);
		}
		else
		{
			if (imgHeight * pixmap.width() > pixmap.height() * imgWidth)
				imgHeight = pixmap.height() * imgWidth / pixmap.width();
			else
				imgWidth = pixmap.width() * imgHeight / pixmap.height();

			m_ui.cover->setPixmap(pixmap.scaled(imgWidth, imgHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		}

		m_ui.coverArea->setMinimumWidth(imgWidth);
		m_ui.coverArea->setMaximumWidth(imgWidth);
	}

private: // QObject
	bool eventFilter(QObject * obj, QEvent * event) override
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

	void OnAnnotationChanged(const IAnnotationController::IDataProvider & dataProvider) override
	{
		auto annotation = m_annotationController->CreateAnnotation(dataProvider);

		const auto addRate = [&] (const char * name, const int column)
		{
			const auto & book = dataProvider.GetBook();
			const auto rate = book.GetRawData(column).toInt();
			if (rate <= 0 || rate > 5)
				return;

			const auto byteArray = [this, rate]{
				const auto stars = m_rateStarsProvider->GetStars(rate);
				QByteArray bytes;
				QBuffer buffer(&bytes);
				stars.save(&buffer, "PNG");
				return bytes;
			}();

			annotation.replace(QString("@%1@").arg(name), QString(R"(<img src="data:image/png;base64,%1"/>)").arg(byteArray.toBase64()));
		};
		addRate(Loc::RATE, BookItem::Column::LibRate);
		addRate(Loc::USER_RATE, BookItem::Column::UserRate);

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
			m_contentModel.reset(IModelProvider::Lock(m_modelProvider)->CreateTreeModel(dataProvider.GetContent(), *this));
			m_ui.content->setModel(m_contentModel.get());
		}

		OnResize();
	}

	void OnArchiveParserProgress(const int percents) override
	{
		m_forwarder.Forward([&, percents]
		{
			if (percents)
			{
				if (!m_progressTimer.isActive())
					m_ui.info->setText(QString("%1%").arg(percents));
			}
			else
			{
				m_progressTimer.start();
			}
		});
	}

private:
	void OnLinkActivated(const QString & link)
	{
		const auto url = link.split("://");
		assert(url.size() == 2);
		if (std::ranges::none_of(CUSTOM_URL_SCHEMA, [&] (const char * schema)
		{
			return QString(schema).startsWith(url.front());
		}))
		{
			QDesktopServices::openUrl(link);
			return;
		}

		m_settings->Set(QString(Constant::Settings::RECENT_NAVIGATION_ID_KEY).arg(m_currentCollectionId).arg(url.front()), url.back());
		m_navigationController->SetMode(url.front());
	}

private:
	AnnotationWidget & m_self;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<IAnnotationController, std::shared_ptr> m_annotationController;
	std::weak_ptr<const IModelProvider> m_modelProvider;
	std::weak_ptr<const ILogicFactory> m_logicFactory;
	PropagateConstPtr<IUiFactory, std::shared_ptr> m_uiFactory;
	PropagateConstPtr<IBooksExtractorProgressController, std::shared_ptr> m_progressController;
	std::shared_ptr<const IRateStarsProvider> m_rateStarsProvider;
	PropagateConstPtr<ITreeViewController, std::shared_ptr> m_navigationController;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_contentModel{ std::shared_ptr<QAbstractItemModel>{} };
	Ui::AnnotationWidget m_ui {};
	IAnnotationController::IDataProvider::Covers m_covers;
	QSvgRenderer m_svgRenderer { QString(":/icons/unsupported_image.svg") };
	const QString m_currentCollectionId;
	std::optional<size_t> m_coverIndex;
	std::optional<size_t> m_currentCoverIndex;
	bool m_showContent { true };
	bool m_showCover { true };
	Util::FunctorExecutionForwarder m_forwarder;
	QTimer m_progressTimer;
};

AnnotationWidget::AnnotationWidget(std::shared_ptr<ISettings> settings
	, std::shared_ptr<IAnnotationController> annotationController
	, const std::shared_ptr<const IModelProvider>& modelProvider
	, const std::shared_ptr<const ILogicFactory>& logicFactory
	, std::shared_ptr<IUiFactory> uiFactory
	, std::shared_ptr<IBooksExtractorProgressController> progressController
	, const std::shared_ptr<ICollectionController> & collectionController
	, std::shared_ptr<const IRateStarsProvider> rateStarsProvider
	, QWidget * parent
)
	: QWidget(parent)
	, m_impl(*this
		, std::move(settings)
		, std::move(annotationController)
		, modelProvider
		, logicFactory
		, std::move(uiFactory)
		, std::move(progressController)
		, collectionController
		, std::move(rateStarsProvider)
	)
{
	PLOGD << "AnnotationWidget created";
}

AnnotationWidget::~AnnotationWidget()
{
	PLOGD << "AnnotationWidget destroyed";
}

void AnnotationWidget::ShowContent(const bool value)
{
	m_impl->ShowContent(value);
}

void AnnotationWidget::ShowCover(const bool value)
{
	m_impl->ShowCover(value);
}
