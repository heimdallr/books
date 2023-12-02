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
#include "interface/ui/IUiFactory.h"
#include "logic/data/DataItem.h"
#include "logic/model/IModelObserver.h"
#include "logic/TreeViewController/AbstractTreeViewController.h"
#include "util/FunctorExecutionForwarder.h"
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
constexpr auto UPDATED = QT_TRANSLATE_NOOP("Annotation", "Updated:");
constexpr auto SELECT_IMAGE_FILE_NAME = QT_TRANSLATE_NOOP("Annotation", "Select image file name");
constexpr auto IMAGE_FILE_NAME_FILTER = QT_TRANSLATE_NOOP("Annotation", "Jpeg images (*.jpg *.jpeg);;PNG images (*.png);;All files (*.*)");

constexpr auto SPLITTER_KEY = "ui/Annotation/Splitter";

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

}

class AnnotationWidget::Impl final
	: IAnnotationController::IObserver
	, IModelObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(AnnotationWidget & self
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<IAnnotationController> annotationController
		, std::shared_ptr<IModelProvider> modelProvider
		, std::shared_ptr<IUiFactory> uiFactory
		, const std::shared_ptr<ICollectionController> & collectionController
		, const std::shared_ptr<ILogicFactory> & logicFactory
	)
		: m_self(self)
		, m_settings(std::move(settings))
		, m_annotationController(std::move(annotationController))
		, m_modelProvider(std::move(modelProvider))
		, m_uiFactory(std::move(uiFactory))
		, m_navigationController(std::shared_ptr<ITreeViewController>(logicFactory->GetTreeViewController(ItemType::Navigation)))
		, m_currentCollectionId(collectionController->GetActiveCollectionId())
	{
		m_ui.setupUi(&m_self);
		m_ui.cover->setVisible(false);
		m_progressTimer.setSingleShot(true);
		m_progressTimer.setInterval(std::chrono::milliseconds(300));

		if (const auto value = m_settings->Get(SPLITTER_KEY); value.isValid())
			m_ui.splitter->restoreState(value.toByteArray());

		m_ui.scrollArea->viewport()->setStyleSheet("QWidget { background-color: white }");

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

		m_ui.cover->addAction(m_ui.actionSavePictureAs);
		m_ui.cover->addAction(m_ui.actionCopyImage);

		connect(m_ui.actionSavePictureAs, &QAction::triggered, &m_self, [&]
		{
			assert(!m_covers.empty());

			const auto fileName = m_uiFactory->GetSaveFileName(Tr(SELECT_IMAGE_FILE_NAME), {}, IMAGE_FILE_NAME_FILTER);

			QPixmap pixmap;
			[[maybe_unused]] const auto ok = pixmap.loadFromData(m_covers[m_currentCoverIndex]);
			pixmap.save(fileName);
		});

		connect(m_ui.actionCopyImage, &QAction::triggered, &m_self, [&]
		{
			assert(!m_covers.empty());

			QPixmap pixmap;
			[[maybe_unused]] const auto ok = pixmap.loadFromData(m_covers[m_currentCoverIndex]);
			QGuiApplication::clipboard()->setImage(pixmap.toImage());
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
		m_ui.cover->setVisible(m_showCover);

		if (m_covers.empty() || !m_ui.cover->isVisible())
			return;

		const auto maxHeight = m_ui.mainWidget->height();
		const auto maxWidth = m_ui.scrollArea->width() / 2;

		QPixmap pixmap;
		[[maybe_unused]] const auto ok = pixmap.loadFromData(m_covers[m_currentCoverIndex]);
		m_ui.cover->setPixmap(pixmap.scaled(maxWidth, maxHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	}

private: // IAnnotationController::IObserver
	void OnAnnotationRequested() override
	{
		m_ui.contentWidget->setVisible(false);
		m_ui.content->setModel(nullptr);
		m_ui.cover->setVisible(false);
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

		Add(annotation, Table()
			.Add(FILENAME, dataProvider.GetBook().GetRawData(BookItem::Column::FileName))
			.Add(SIZE, QString("%L1").arg(dataProvider.GetBook().GetRawData(BookItem::Column::Size).toLongLong()))
			.Add(UPDATED, dataProvider.GetBook().GetRawData(BookItem::Column::UpdateDate))
			.ToString());

		m_ui.info->setText(annotation);

		m_covers = dataProvider.GetCovers();
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
	PropagateConstPtr<IUiFactory, std::shared_ptr> m_uiFactory;
	PropagateConstPtr<ITreeViewController, std::shared_ptr> m_navigationController;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_contentModel{ std::shared_ptr<QAbstractItemModel>{} };
	Ui::AnnotationWidget m_ui {};
	std::vector<QByteArray> m_covers;
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
	, std::shared_ptr<IUiFactory> uiFactory
	, const std::shared_ptr<ICollectionController> & collectionController
	, const std::shared_ptr<ILogicFactory> & logicFactory
	, QWidget * parent
)
	: QWidget(parent)
	, m_impl(*this
		, std::move(settings)
		, std::move(annotationController)
		, std::move(modelProvider)
		, std::move(uiFactory)
		, collectionController
		, logicFactory)
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

void AnnotationWidget::resizeEvent(QResizeEvent *)
{
	m_impl->OnResize();
}
