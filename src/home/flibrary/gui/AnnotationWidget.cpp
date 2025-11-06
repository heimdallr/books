#include "ui_AnnotationWidget.h"

#include "AnnotationWidget.h"

#include <QBuffer>
#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QMenu>
#include <QPainter>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>

#include "fnd/FindPair.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ProductConstant.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IDataItem.h"

#include "gutil/util.h"
#include "logic/TreeViewController/AbstractTreeViewController.h"
#include "logic/data/DataItem.h"
#include "logic/shared/ImageRestore.h"
#include "util/FunctorExecutionForwarder.h"
#include "util/IExecutor.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr std::pair<const char*, bool> NO_NAVIGATION { nullptr, false };

constexpr std::pair<const char*, std::pair<const char*, bool>> TYPE_TO_NAVIGATION[] {
	{	  Loc::AUTHORS,      { Loc::Authors, true } },
    {       Loc::SERIES,       { Loc::Series, true } },
    {       Loc::GENRES,       { Loc::Genres, true } },
    { Loc::PUBLISH_YEAR, { Loc::PublishYears, true } },
	{     Loc::KEYWORDS,     { Loc::Keywords, true } },
    {      Loc::UPDATES,      { Loc::Updates, true } },
    {      Loc::ARCHIVE,     { Loc::Archives, true } },
    {     Loc::LANGUAGE,    { Loc::Languages, true } },
	{	   Loc::GROUPS,       { Loc::Groups, true } },
    {          "Search",      { Loc::Search, false } },
    {         "Reviews",     { Loc::Reviews, false } },
    {        "AllBooks",    { Loc::AllBooks, false } },
};
static_assert(std::size(TYPE_TO_NAVIGATION) == static_cast<size_t>(NavigationMode::Last));

constexpr auto CONTEXT                   = "Annotation";
constexpr auto SELECT_IMAGE_FILE_NAME    = QT_TRANSLATE_NOOP("Annotation", "Select image file name");
constexpr auto SELECT_IMAGE_FOLDER       = QT_TRANSLATE_NOOP("Annotation", "Select images folder");
constexpr auto IMAGE_FILE_NAME_FILTER    = QT_TRANSLATE_NOOP("Annotation", "Jpeg images (*.jpg *.jpeg);;PNG images (*.png);;All files (*.*)");
constexpr auto SAVE_ALL_PICS_ACTION_TEXT = QT_TRANSLATE_NOOP("Annotation", "Save &all images (%1)...");
constexpr auto SAVED_ALL                 = QT_TRANSLATE_NOOP("Annotation", "All %1 images were successfully saved");
constexpr auto SAVED_PARTIALLY           = QT_TRANSLATE_NOOP("Annotation", "%1 out of %2 images were saved");
constexpr auto SAVED_WITH_ERRORS         = QT_TRANSLATE_NOOP("Annotation", "%1 images out of %2 could not be saved");
constexpr auto CANNOT_SAVE_IMAGE         = QT_TRANSLATE_NOOP("Annotation", "Cannot save image to %1");
constexpr auto CANNOT_OPEN_IMAGE         = QT_TRANSLATE_NOOP("Annotation", "Cannot open %1");
constexpr auto SHOW_FILE_METADATA        = QT_TRANSLATE_NOOP("Annotation", "Show file metadata");
constexpr auto SHOW_CONTENT              = QT_TRANSLATE_NOOP("Annotation", "Show content");

#if (false) // NOLINT(readability-avoid-unconditional-preprocessor-if)
QT_TRANSLATE_NOOP("Annotation", "Read")
QT_TRANSLATE_NOOP("Annotation", "AsIs")
QT_TRANSLATE_NOOP("Annotation", "Archive")
QT_TRANSLATE_NOOP("Annotation", "Script")
QT_TRANSLATE_NOOP("Annotation", "Inpx")
#endif

constexpr auto DIALOG_KEY = "Image";

TR_DEF

bool SaveImage(QString& fileName, const QByteArray& bytes)
{
	const auto [recoded, mediaType] = Recode(bytes);
	if (const QFileInfo fileInfo(fileName); fileInfo.suffix().isEmpty())
		fileName += QString(mediaType) == IMAGE_PNG ? ".png" : ".jpg";

	if (QFile::exists(fileName))
		PLOGW << fileName << " already exists and will be overwritten";

	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly))
	{
		PLOGE << "Cannot open to write into " << fileName;
		return false;
	}

	if (file.write(recoded) != recoded.size())
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

constexpr auto CONTENT_MODE_KEY = "ui/View/AnnotationContentMode";
#define CONTENT_MODE_ITEMS_X_MACRO \
	CONTENT_MODE_ITEM(Content)     \
	CONTENT_MODE_ITEM(Description)

enum class ContentMode
{
#define CONTENT_MODE_ITEM(NAME) NAME,
	CONTENT_MODE_ITEMS_X_MACRO
#undef CONTENT_MODE_ITEM
};

constexpr std::pair<const char*, ContentMode> CONTENT_MODE_NAMES[] {
#define CONTENT_MODE_ITEM(NAME) { #NAME, ContentMode::NAME },
	CONTENT_MODE_ITEMS_X_MACRO
#undef CONTENT_MODE_ITEM
};

} // namespace

class AnnotationWidget::Impl final
	: QObject
	, IAnnotationController::IObserver
	, IAnnotationController::IStrategy
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(
		AnnotationWidget&                                  self,
		const std::shared_ptr<const IModelProvider>&       modelProvider,
		const std::shared_ptr<const ILogicFactory>&        logicFactory,
		const std::shared_ptr<ICollectionController>&      collectionController,
		std::shared_ptr<const IBookInteractor>             bookInteractor,
		std::shared_ptr<ISettings>                         settings,
		std::shared_ptr<IAnnotationController>             annotationController,
		std::shared_ptr<IUiFactory>                        uiFactory,
		std::shared_ptr<IBooksExtractorProgressController> progressController,
		std::shared_ptr<ItemViewToolTipper>                itemViewToolTipperContent,
		std::shared_ptr<ScrollBarController>               scrollBarControllerContent,
		std::shared_ptr<ScrollBarController>               scrollBarControllerAnnotation
	)
		: m_self { self }
		, m_bookInteractor { std::move(bookInteractor) }
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

		const auto setCustomPalette = [](QWidget& widget) {
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

		m_ui.cover->setStyleSheet("background-color: white;");

		connect(m_ui.info, &QLabel::linkActivated, m_ui.info, [&](const QString& link) {
			OnLinkActivated(link);
		});
		connect(m_ui.info, &QLabel::linkHovered, m_ui.info, [&](const QString& link) {
			PLOGI_IF(!link.isEmpty()) << link;
		});

		const auto onCoverClicked = [&](const QPoint& pos) {
			if (m_covers.size() < 2)
				return;

			switch (3 * pos.x() / m_ui.cover->width())
			{
				case 0:
					if (m_currentCoverIndex == 0)
						m_currentCoverIndex = m_covers.size() - 1;
					else
						--m_currentCoverIndex;
					break;

				case 1:
					m_currentCoverIndex = 0;
					m_coverButtons[CoverButtonType::Home]->setVisible(false);
					break;

				case 2:
					if (++m_currentCoverIndex >= m_covers.size())
						m_currentCoverIndex = 0;
					break;

				default:
					assert(false && "wtf?");
			}

			OnResize();
		};
		connect(m_ui.cover, &ClickableLabel::clicked, onCoverClicked);
		connect(m_ui.cover, &QWidget::customContextMenuRequested, &m_self, [this](const QPoint& pos) {
			QMenu menu;
			menu.addAction(m_ui.actionOpenImage);
			menu.addAction(m_ui.actionCopyImage);
			menu.addAction(m_ui.actionSaveImageAs);
			menu.addAction(m_ui.actionSaveAllImages);
			menu.setFont(m_self.font());
			menu.exec(m_ui.cover->mapToGlobal(pos));
		});

		connect(m_ui.content, &QWidget::customContextMenuRequested, &m_self, [this](const QPoint& pos) {
			QMenu menu;
			menu.setFont(m_self.font());

			const std::pair<ContentMode, std::tuple<const char*, QAbstractItemModel*, ContentMode, bool>> modes[] {
				{     ContentMode::Content, { SHOW_FILE_METADATA, m_descriptionModel.get(), ContentMode::Description, true } },
				{ ContentMode::Description,              { SHOW_CONTENT, m_contentModel.get(), ContentMode::Content, false } },
			};

			const auto& [title, model, mode, expand] = FindSecond(modes, m_contentMode, modes[0].second);
			auto* action                             = menu.addAction(Tr(title));
			connect(action, &QAction::triggered, [&] {
				m_ui.content->setModel(model);
				m_contentMode = mode;
				m_settings->Set(CONTENT_MODE_KEY, CONTENT_MODE_NAMES[static_cast<size_t>(m_contentMode)].first);
				if (expand)
					m_ui.content->expandAll();
			});
			action->setEnabled(!!model);

			Util::FillTreeContextMenu(*m_ui.content, menu).exec(m_ui.content->mapToGlobal(pos));
		});

		const auto openImage = [this] {
			assert(!m_covers.empty());
			const auto& [name, bytes] = m_covers[m_currentCoverIndex];
			auto path                 = m_logicFactory.lock()->CreateTemporaryDir()->filePath(name);

			if (!SaveImage(path, bytes))
				return m_uiFactory->ShowError(Tr(CANNOT_SAVE_IMAGE).arg(path));
			if (!QDesktopServices::openUrl(path))
				m_uiFactory->ShowError(Tr(CANNOT_OPEN_IMAGE).arg(path));
		};
		connect(m_ui.cover, &ClickableLabel::doubleClicked, &m_self, openImage);

		connect(m_ui.cover, &ClickableLabel::mouseEnter, &m_self, [this] {
			OnCoverEnter();
		});
		connect(m_ui.cover, &ClickableLabel::mouseLeave, &m_self, [this] {
			OnCoverLeave();
		});

		connect(m_ui.actionOpenImage, &QAction::triggered, &m_self, openImage);

		connect(m_ui.actionCopyImage, &QAction::triggered, &m_self, [this] {
			assert(!m_covers.empty());
			const auto pixmap = Decode(m_covers[m_currentCoverIndex].bytes);
			QGuiApplication::clipboard()->setImage(pixmap.toImage());
		});

		connect(m_ui.actionSaveImageAs, &QAction::triggered, &m_self, [this] {
			assert(!m_covers.empty());
			if (auto fileName = m_uiFactory->GetSaveFileName(DIALOG_KEY, Tr(SELECT_IMAGE_FILE_NAME), IMAGE_FILE_NAME_FILTER); !fileName.isEmpty())
				SaveImage(fileName, m_covers[m_currentCoverIndex].bytes);
		});

		connect(m_ui.actionSaveAllImages, &QAction::triggered, &m_self, [this] {
			auto folder = m_uiFactory->GetExistingDirectory(DIALOG_KEY, Tr(SELECT_IMAGE_FOLDER));
			if (folder.isEmpty())
				return;

			std::shared_ptr progressItem = m_progressController->Add(static_cast<int>(m_covers.size()));
			std::shared_ptr executor     = ILogicFactory::Lock(m_logicFactory)->GetExecutor();

			(*executor)({ "Save images", [this, executor, folder = std::move(folder), covers = m_covers, progressItem = std::move(progressItem)]() mutable {
							 size_t savedCount = 0;
							 for (const auto& [name, bytes] : covers)
							 {
								 auto path = QString("%1/%2").arg(folder).arg(name);
								 if (SaveImage(path, bytes))
									 ++savedCount;

								 progressItem->Increment(1);
								 if (progressItem->IsStopped())
									 break;
							 }

							 return [this, executor = std::move(executor), progressItem = std::move(progressItem), savedCount, totalCount = covers.size()](size_t) mutable {
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

		const auto createCoverButton = [this, onCoverClicked](const QString& iconFileName, const QAction* action) {
			auto* btn = new QToolButton(&m_self);
			btn->setVisible(false);
			btn->setToolTip(action->shortcut().toString());
			btn->setIcon(QIcon(iconFileName));
			connect(btn, &QAbstractButton::clicked, &m_self, [this, onCoverClicked] {
				onCoverClicked(m_ui.cover->mapFromGlobal(QCursor::pos()));
			});
			m_coverButtons.push_back(btn);
		};

		createCoverButton(":/icons/left.svg", m_ui.actionImagePrev);
		createCoverButton(":/icons/center.svg", m_ui.actionImageHome);
		createCoverButton(":/icons/right.svg", m_ui.actionImageNext);

		m_ui.cover->addActions({ m_ui.actionImagePrev, m_ui.actionImageNext, m_ui.actionImageHome });
		connect(m_ui.actionImagePrev, &QAction::triggered, [this, onCoverClicked] {
			onCoverClicked(QPoint(1, 1));
		});
		connect(m_ui.actionImageNext, &QAction::triggered, [this, onCoverClicked] {
			onCoverClicked(QPoint(m_ui.cover->width() - 1, 1));
		});
		connect(m_ui.actionImageHome, &QAction::triggered, [this, onCoverClicked] {
			onCoverClicked(QPoint(m_ui.cover->width() / 2, 1));
		});
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
		auto imgWidth  = m_ui.mainWidget->width() / 3;

		if (auto pixmap = Decode(m_covers[m_currentCoverIndex].bytes); !pixmap.isNull())
		{
			if (imgHeight * pixmap.width() > pixmap.height() * imgWidth)
				imgHeight = pixmap.height() * imgWidth / pixmap.width();
			else
				imgWidth = pixmap.width() * imgHeight / pixmap.height();

			m_ui.cover->setPixmap(pixmap.scaled(imgWidth, imgHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		}
		else
		{
			QSvgRenderer renderer(QString(":/icons/unsupported-image.svg"));
			const auto   defaultSize = renderer.defaultSize();
			imgWidth                 = imgHeight * defaultSize.width() / defaultSize.height();
			pixmap                   = QPixmap(imgWidth, imgHeight);
			pixmap.fill(Qt::transparent);
			QPainter painter(&pixmap);
			renderer.render(&painter);
			m_ui.cover->setPixmap(pixmap);
		}

		if (m_covers.size() > 1)
			m_coverButtonsEnabled = true;

		m_ui.coverArea->setMinimumWidth(imgWidth);
		m_ui.coverArea->setMaximumWidth(imgWidth);

		const QFontMetrics metrics(m_self.font());
		const auto         height = 3 * metrics.lineSpacing() / 2;
		const QSize        size { height, height };
		const auto         top = imgHeight - height - height / 8;
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
		m_currentCoverIndex = 0;
		m_contentModel.reset();
		m_descriptionModel.reset();
	}

	void OnAnnotationChanged(const IAnnotationController::IDataProvider& dataProvider) override
	{
		auto annotation = m_annotationController->CreateAnnotation(dataProvider, *this);

		m_ui.info->setText(annotation);
		m_covers = dataProvider.GetCovers();
		m_ui.actionSaveAllImages->setText(Tr(SAVE_ALL_PICS_ACTION_TEXT).arg(m_covers.size()));
		if (m_covers.empty())
			return;

		m_currentCoverIndex = 0;

		if (m_covers.size() > 1)
			m_ui.cover->setCursor(Qt::PointingHandCursor);

		if (dataProvider.GetContent()->GetChildCount() > 0)
		{
			if (m_showContent)
				m_ui.contentWidget->setVisible(true);
			m_contentModel.reset(IModelProvider::Lock(m_modelProvider)->CreateTreeModel(dataProvider.GetContent(), true));
		}

		if (dataProvider.GetDescription()->GetChildCount() > 0)
		{
			if (m_showContent)
				m_ui.contentWidget->setVisible(true);
			m_descriptionModel.reset(IModelProvider::Lock(m_modelProvider)->CreateTreeModel(dataProvider.GetDescription(), true));
		}

		switch (m_contentMode)
		{
			case ContentMode::Content:
				if (m_contentModel)
				{
					m_ui.content->setModel(m_contentModel.get());
				}
				else if (m_descriptionModel)
				{
					m_ui.content->setModel(m_descriptionModel.get());
					m_ui.content->expandAll();
				}
				break;

			case ContentMode::Description:
				if (m_descriptionModel)
				{
					m_ui.content->setModel(m_descriptionModel.get());
					m_ui.content->expandAll();
				}
				else if (m_contentModel)
				{
					m_ui.content->setModel(m_contentModel.get());
				}
				break;
		}

		OnResize();
	}

	void OnJokeTextChanged(const QString& value) override
	{
		m_ui.info->setText(value);
	}

	void OnJokeImageChanged(const QByteArray& value) override
	{
		m_covers.emplace_back("", value);
		m_currentCoverIndex = 0;
		OnResize();
	}

	void OnArchiveParserProgress(const int percents) override
	{
		m_forwarder.Forward([&, percents] {
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
	QString GenerateUrl(const char* type, const QString& id, const QString& str, const bool textMode) const override
	{
		if (str.isEmpty())
			return {};

		const auto& navigation = FindSecond(TYPE_TO_NAVIGATION, type, NO_NAVIGATION, PszComparer {});
		return textMode || (navigation.first && !m_settings->Get(QString(Constant::Settings::VIEW_NAVIGATION_KEY_TEMPLATE).arg(navigation.first), true)) ? QString("%1").arg(str)
		                                                                                                                                                 : QString("<a href=%1//%2>%3</a>").arg(type, id, str);
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
			return m_bookInteractor->OnLinkActivated(url.back().toLongLong());
		}
		if (std::ranges::none_of(TYPE_TO_NAVIGATION, [&](const auto& schema) {
				return schema.second.second && QString(schema.first).startsWith(url.front());
			}))
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

		m_coverButtons[CoverButtonType::Previous]->setVisible(true);
		m_coverButtons[CoverButtonType::Next]->setVisible(true);
		m_coverButtons[CoverButtonType::Home]->setVisible(m_currentCoverIndex != 0);
	}

	void OnCoverLeave() const
	{
		if (std::ranges::any_of(m_coverButtons, [widget = QApplication::widgetAt(QCursor::pos())](const auto* item) {
				return widget == item;
			}))
			return;

		for (auto* btn : m_coverButtons)
			btn->setVisible(false);
	}

private:
	AnnotationWidget&                                                     m_self;
	std::shared_ptr<const IBookInteractor>                                m_bookInteractor;
	PropagateConstPtr<ISettings, std::shared_ptr>                         m_settings;
	PropagateConstPtr<IAnnotationController, std::shared_ptr>             m_annotationController;
	std::weak_ptr<const IModelProvider>                                   m_modelProvider;
	std::weak_ptr<const ILogicFactory>                                    m_logicFactory;
	PropagateConstPtr<IUiFactory, std::shared_ptr>                        m_uiFactory;
	PropagateConstPtr<IBooksExtractorProgressController, std::shared_ptr> m_progressController;
	PropagateConstPtr<ItemViewToolTipper, std::shared_ptr>                m_itemViewToolTipperContent;
	PropagateConstPtr<ScrollBarController, std::shared_ptr>               m_scrollBarControllerContent;
	PropagateConstPtr<ScrollBarController, std::shared_ptr>               m_scrollBarControllerAnnotation;

	PropagateConstPtr<ITreeViewController, std::shared_ptr> m_navigationController;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr>  m_contentModel { std::shared_ptr<QAbstractItemModel> {} };
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr>  m_descriptionModel { std::shared_ptr<QAbstractItemModel> {} };

	Ui::AnnotationWidget                         m_ui {};
	IAnnotationController::IDataProvider::Covers m_covers;
	const QString                                m_currentCollectionId;
	size_t                                       m_currentCoverIndex { 0 };
	bool                                         m_showContent { true };
	bool                                         m_showCover { true };
	Util::FunctorExecutionForwarder              m_forwarder;
	QTimer                                       m_progressTimer;
	ContentMode m_contentMode { FindSecond(CONTENT_MODE_NAMES, m_settings->Get(CONTENT_MODE_KEY, QString {}).toStdString().data(), CONTENT_MODE_NAMES[0].second, PszComparer {}) };

	std::vector<QAbstractButton*> m_coverButtons;
	bool                          m_coverButtonsEnabled { false };
	bool                          m_coverButtonsVisible { true };
	const int                     m_starSymbol { m_settings->Get(Constant::Settings::LIBRATE_STAR_SYMBOL_KEY, Constant::Settings::LIBRATE_STAR_SYMBOL_DEFAULT) };
};

AnnotationWidget::AnnotationWidget(
	const std::shared_ptr<const IModelProvider>&       modelProvider,
	const std::shared_ptr<const ILogicFactory>&        logicFactory,
	const std::shared_ptr<ICollectionController>&      collectionController,
	std::shared_ptr<const IBookInteractor>             bookInteractor,
	std::shared_ptr<ISettings>                         settings,
	std::shared_ptr<IAnnotationController>             annotationController,
	std::shared_ptr<IUiFactory>                        uiFactory,
	std::shared_ptr<IBooksExtractorProgressController> progressController,
	std::shared_ptr<ItemViewToolTipper>                itemViewToolTipperContent,
	std::shared_ptr<ScrollBarController>               scrollBarControllerContent,
	std::shared_ptr<ScrollBarController>               scrollBarControllerAnnotation,
	QWidget*                                           parent
)
	: QWidget(parent)
	, m_impl(
		  *this,
		  modelProvider,
		  logicFactory,
		  collectionController,
		  std::move(bookInteractor),
		  std::move(settings),
		  std::move(annotationController),
		  std::move(uiFactory),
		  std::move(progressController),
		  std::move(itemViewToolTipperContent),
		  std::move(scrollBarControllerContent),
		  std::move(scrollBarControllerAnnotation)
	  )
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
