#include "ui_AnnotationWidget.h"
#include "AnnotationWidget.h"

#include <ranges>

#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QTimer>

#include <plog/Log.h>

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/IDataItem.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IModelProvider.h"
#include "interface/logic/IProgressController.h"
#include "interface/ui/IUiFactory.h"
#include "logic/data/DataItem.h"
#include "logic/model/IModelObserver.h"
#include "logic/TreeViewController/AbstractTreeViewController.h"
#include "util/FunctorExecutionForwarder.h"
#include "util/IExecutor.h"
#include "util/ISettings.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTEXT = "Annotation";
constexpr auto KEYWORDS = QT_TRANSLATE_NOOP("Annotation", "Keywords: %1");
constexpr auto AUTHORS = QT_TRANSLATE_NOOP("Annotation", "Authors:");
constexpr auto SERIES = QT_TRANSLATE_NOOP("Annotation", "Series:");
constexpr auto GENRES = QT_TRANSLATE_NOOP("Annotation", "Genres:");
constexpr auto ARCHIVE = QT_TRANSLATE_NOOP("Annotation", "Archives:");
constexpr auto GROUPS = QT_TRANSLATE_NOOP("Annotation", "Groups:");
constexpr auto FILENAME = QT_TRANSLATE_NOOP("Annotation", "File:");
constexpr auto SIZE = QT_TRANSLATE_NOOP("Annotation", "Size:");
constexpr auto IMAGES = QT_TRANSLATE_NOOP("Annotation", "Images:");
constexpr auto UPDATED = QT_TRANSLATE_NOOP("Annotation", "Updated:");
constexpr auto TRANSLATORS = QT_TRANSLATE_NOOP("Annotation", "Translators:");
constexpr auto SELECT_IMAGE_FILE_NAME = QT_TRANSLATE_NOOP("Annotation", "Select image file name");
constexpr auto SELECT_IMAGE_FOLDER = QT_TRANSLATE_NOOP("Annotation", "Select images folder");
constexpr auto IMAGE_FILE_NAME_FILTER = QT_TRANSLATE_NOOP("Annotation", "Jpeg images (*.jpg *.jpeg);;PNG images (*.png);;All files (*.*)");
constexpr auto SAVE_ALL_PICS_ACTION_TEXT = QT_TRANSLATE_NOOP("Annotation", "Save &all pictures (%1)...");
constexpr auto SAVED_ALL = QT_TRANSLATE_NOOP("Annotation", "All %1 pictures were successfully saved");
constexpr auto SAVED_PARTIALLY = QT_TRANSLATE_NOOP("Annotation", "%1 out of %2 pictures were saved");
constexpr auto SAVED_WITH_ERRORS = QT_TRANSLATE_NOOP("Annotation", "%1 pictures out of %2 could not be saved");

constexpr auto SPLITTER_KEY = "ui/Annotation/Splitter";
constexpr auto DIALOG_KEY = "Image";

constexpr auto ERROR_PATTERN = R"(<p style="font-style:italic;">%1</p>)";
constexpr auto TITLE_PATTERN = "<p align=center><b>%1</b></p>";
constexpr auto EPIGRAPH_PATTERN = R"(<p align=right style="font-style:italic;">%1</p>)";

constexpr const char * CUSTOM_URL_SCHEMA[]
{
	AUTHORS,
	SERIES,
	GENRES,
	ARCHIVE,
	GROUPS,
};

TR_DEF

QString Join(const std::vector<QString> & strings, const QString & delimiter = ", ")
{
	if (strings.empty())
		return {};

	auto result = strings.front();
	std::ranges::for_each(strings | std::views::drop(1), [&] (const QString & item)
	{
		result.append(delimiter).append(item);
	});

	return result;
}

void Add(QString & text, const QString & str, const char * pattern = "%1")
{
	if (!str.isEmpty())
		text.append(QString("<p>%1</p>").arg(Tr(pattern).arg(str)));
}

QString Url(const char * type, const QString & id, const QString & str)
{
	return str.isEmpty() ? QString{} : QString("<a href=%1//%2>%3</a>").arg(type, id, str);
}

QString Urls(const char * type, const IDataItem & parent)
{
	std::vector<QString> urls;
	for (size_t i = 0, sz = parent.GetChildCount(); i < sz; ++i)
	{
		const auto item = parent.GetChild(i);
		urls.push_back(Url(type, item->GetId(), item->GetData(DataItem::Column::Title)));
	}

	return Join(urls);
}

struct Table
{
	Table & Add(const char * name, const QString & value)
	{
		if (!value.isEmpty())
			data << QString("<tr><td>%1</td><td>%2</td></tr>").arg(Tr(name)).arg(value);

		return *this;
	}

	[[nodiscard]] QString ToString() const
	{
		return data.isEmpty() ? QString {} : QString("<table>%1</table>\n").arg(data.join("\n"));
	}

	QStringList data;
};

bool SaveImage(const QString & fileName, const QByteArray & bytes)
{
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
		, std::shared_ptr<IModelProvider> modelProvider
		, std::shared_ptr<ILogicFactory> logicFactory
		, std::shared_ptr<IUiFactory> uiFactory
		, std::shared_ptr<IBooksExtractorProgressController> progressController
		, const std::shared_ptr<ICollectionController> & collectionController
	)
		: m_self(self)
		, m_settings(std::move(settings))
		, m_annotationController(std::move(annotationController))
		, m_modelProvider(std::move(modelProvider))
		, m_logicFactory(std::move(logicFactory))
		, m_uiFactory(std::move(uiFactory))
		, m_progressController(std::move(progressController))
		, m_navigationController(m_logicFactory->GetTreeViewController(ItemType::Navigation))
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
					if (--m_currentCoverIndex < 0)
						m_currentCoverIndex = static_cast<int>(m_covers.size()) - 1;
					break;

				case 1:
					m_currentCoverIndex = m_coverIndex;
					break;

				case 2:
					if (++m_currentCoverIndex >= static_cast<int>(m_covers.size()))
						m_currentCoverIndex = 0;
					break;

				default:
					assert(false && "wtf?");
			}

			OnResize();
		});

		m_ui.cover->addAction(m_ui.actionCopyImage);
		m_ui.cover->addAction(m_ui.actionSavePictureAs);
		m_ui.cover->addAction(m_ui.actionSaveAllPictures);

		connect(m_ui.actionCopyImage, &QAction::triggered, &m_self, [&]
		{
			assert(!m_covers.empty());

			QPixmap pixmap;
			[[maybe_unused]] const auto ok = pixmap.loadFromData(m_covers[m_currentCoverIndex].bytes);
			QGuiApplication::clipboard()->setImage(pixmap.toImage());
		});

		connect(m_ui.actionSavePictureAs, &QAction::triggered, &m_self, [&]
		{
			assert(!m_covers.empty());
			if (const auto fileName = m_uiFactory->GetSaveFileName(DIALOG_KEY, Tr(SELECT_IMAGE_FILE_NAME), IMAGE_FILE_NAME_FILTER); !fileName.isEmpty())
				SaveImage(fileName, m_covers[m_currentCoverIndex].bytes);
		});

		connect(m_ui.actionSaveAllPictures, &QAction::triggered, &m_self, [&]
		{
			auto folder = m_uiFactory->GetExistingDirectory(DIALOG_KEY, Tr(SELECT_IMAGE_FOLDER));
			if (folder.isEmpty())
				return;

			std::shared_ptr progressItem = m_progressController->Add(static_cast<int>(m_covers.size()));
			std::shared_ptr executor = m_logicFactory->GetExecutor();

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

	void OnResize() const
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
		[[maybe_unused]] const auto ok = pixmap.loadFromData(m_covers[m_currentCoverIndex].bytes);
		assert(ok);

		if (imgHeight * pixmap.width() > pixmap.height() * imgWidth)
			imgHeight = pixmap.height() * imgWidth / pixmap.width();
		else
			imgWidth = pixmap.width() * imgHeight / pixmap.height();

		m_ui.cover->setPixmap(pixmap.scaled(imgWidth, imgHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
		m_currentCoverIndex = m_coverIndex = -1;
	}

	void OnAnnotationChanged(const IAnnotationController::IDataProvider & dataProvider) override
	{
		QString annotation;
		Add(annotation, dataProvider.GetBook().GetRawData(BookItem::Column::Title), TITLE_PATTERN);
		Add(annotation, dataProvider.GetEpigraph(), EPIGRAPH_PATTERN);
		Add(annotation, dataProvider.GetEpigraphAuthor(), EPIGRAPH_PATTERN);
		Add(annotation, dataProvider.GetAnnotation());
		Add(annotation, Join(dataProvider.GetKeywords()), KEYWORDS);

		Add(annotation, Table()
			.Add(AUTHORS, Urls(AUTHORS, dataProvider.GetAuthors()))
			.Add(SERIES, Url(SERIES, dataProvider.GetSeries().GetId(), dataProvider.GetSeries().GetRawData(NavigationItem::Column::Title)))
			.Add(GENRES, Urls(GENRES, dataProvider.GetGenres()))
			.Add(ARCHIVE, Url(ARCHIVE, dataProvider.GetBook().GetRawData(BookItem::Column::Folder), dataProvider.GetBook().GetRawData(BookItem::Column::Folder)))
			.Add(GROUPS, Urls(GROUPS, dataProvider.GetGroups()))
			.ToString());

		if (const auto translators = dataProvider.GetTranslators(); translators && translators->GetChildCount() > 0)
		{
			Table table;
			for (size_t i = 0, sz = translators->GetChildCount(); i < sz; ++i)
				table.Add(i == 0 ? TRANSLATORS : "", GetAuthorFull(*translators->GetChild(i)));
			Add(annotation, table.ToString());
		}

		m_covers = dataProvider.GetCovers();
		m_ui.actionSaveAllPictures->setText(Tr(SAVE_ALL_PICS_ACTION_TEXT).arg(m_covers.size()));

		{
			auto info = Table()
				.Add(FILENAME, dataProvider.GetBook().GetRawData(BookItem::Column::FileName))
				.Add(SIZE, QString("%L1").arg(dataProvider.GetBook().GetRawData(BookItem::Column::Size).toLongLong()))
				.Add(UPDATED, dataProvider.GetBook().GetRawData(BookItem::Column::UpdateDate));
			if (!m_covers.empty())
			{
				const auto total = std::accumulate(m_covers.cbegin(), m_covers.cend(), qsizetype { 0 }, [] (const auto init, const auto & cover)
				{
					return init + cover.bytes.size();
				});
				info.Add(IMAGES, QString("%1 (%L2)").arg(m_covers.size()).arg(total));
			}

			Add(annotation, info.ToString());
		}

		Add(annotation, dataProvider.GetError(), ERROR_PATTERN);

		m_ui.info->setText(annotation);

		if (m_covers.empty())
			return;

		if (m_coverIndex = dataProvider.GetCoverIndex(); m_coverIndex < 0)
			m_coverIndex = 0;
		m_currentCoverIndex = m_coverIndex;

		if (m_covers.size() > 1)
			m_ui.cover->setCursor(Qt::PointingHandCursor);

		if (dataProvider.GetContent()->GetChildCount() > 0)
		{
			if (m_showContent)
				m_ui.contentWidget->setVisible(true);
			m_contentModel.reset(m_modelProvider->CreateTreeModel(dataProvider.GetContent(), *this));
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
	PropagateConstPtr<IModelProvider, std::shared_ptr> m_modelProvider;
	PropagateConstPtr<ILogicFactory, std::shared_ptr> m_logicFactory;
	PropagateConstPtr<IUiFactory, std::shared_ptr> m_uiFactory;
	PropagateConstPtr<IBooksExtractorProgressController, std::shared_ptr> m_progressController;
	PropagateConstPtr<ITreeViewController, std::shared_ptr> m_navigationController;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_contentModel{ std::shared_ptr<QAbstractItemModel>{} };
	Ui::AnnotationWidget m_ui {};
	IAnnotationController::IDataProvider::Covers m_covers;
	const QString m_currentCollectionId;
	int m_coverIndex { -1 };
	int m_currentCoverIndex { -1 };
	bool m_showContent { true };
	bool m_showCover { true };
	Util::FunctorExecutionForwarder m_forwarder;
	QTimer m_progressTimer;
};

AnnotationWidget::AnnotationWidget(std::shared_ptr<ISettings> settings
	, std::shared_ptr<IAnnotationController> annotationController
	, std::shared_ptr<IModelProvider> modelProvider
	, std::shared_ptr<ILogicFactory> logicFactory
	, std::shared_ptr<IUiFactory> uiFactory
	, std::shared_ptr<IBooksExtractorProgressController> progressController
	, const std::shared_ptr<ICollectionController> & collectionController
	, QWidget * parent
)
	: QWidget(parent)
	, m_impl(*this
		, std::move(settings)
		, std::move(annotationController)
		, std::move(modelProvider)
		, std::move(logicFactory)
		, std::move(uiFactory)
		, std::move(progressController)
		, collectionController
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
