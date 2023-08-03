#include "ui_AnnotationWidget.h"
#include "AnnotationWidget.h"

#include <ranges>

#include <plog/Log.h>

#include "interface/constants/Localization.h"
#include "interface/logic/IAnnotationController.h"
#include "interface/logic/IDataItem.h"
#include "logic/data/DataItem.h"

#include "util/ISettings.h"

using namespace HomeCompa::Flibrary;

namespace {
constexpr auto CONTEXT = "Annotation";
constexpr auto KEYWORDS = QT_TRANSLATE_NOOP("Annotation", "Keywords: %1");
constexpr auto SERIES = QT_TRANSLATE_NOOP("Annotation", "Series:");
constexpr auto AUTHORS = QT_TRANSLATE_NOOP("Annotation", "Authors:");
constexpr auto GENRES = QT_TRANSLATE_NOOP("Annotation", "Genres:");
constexpr auto ARCHIVE = QT_TRANSLATE_NOOP("Annotation", "Archive:");
constexpr auto FILENAME = QT_TRANSLATE_NOOP("Annotation", "File:");
constexpr auto SIZE = QT_TRANSLATE_NOOP("Annotation", "Size:");
constexpr auto UPDATED = QT_TRANSLATE_NOOP("Annotation", "Updated:");

constexpr auto SPLITTER_KEY = "ui/Annotation/Splitter";

QString Tr(const char * str)
{
	return Loc::Tr(CONTEXT, str);
}

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
		const auto * item = parent.GetChild(i);
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

	QString ToString() const
	{
		return data.isEmpty() ? QString {} : QString("<table>%1</table>").arg(data.join("\n"));
	}

	QStringList data;
};

}

class AnnotationWidget::Impl
	: IAnnotationController::IObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(AnnotationWidget & self
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<IAnnotationController> annotationController
	)
		: m_self(self)
		, m_settings(std::move(settings))
		, m_annotationController(std::move(annotationController))
	{
		m_ui.setupUi(&m_self);
		m_ui.picture->setVisible(false);

		if (const auto value = m_settings->Get(SPLITTER_KEY); value.isValid())
			m_ui.splitter->restoreState(value.toByteArray());

		m_ui.scrollArea->viewport()->setStyleSheet("QWidget { background-color: white }");

		m_annotationController->RegisterObserver(this);
	}

	~Impl() override
	{
		m_settings->Set(SPLITTER_KEY, m_ui.splitter->saveState());
		m_annotationController->UnregisterObserver(this);
	}

private: // IAnnotationController::IObserver
	void OnAnnotationChanged(const IAnnotationController::IDataProvider & dataProvider) override
	{
		auto annotation = QString("<b>%1</b>").arg(dataProvider.GatBook().GetRawData(BookItem::Column::Title));
		Add(annotation, dataProvider.GetAnnotation());
		Add(annotation, Join(dataProvider.GetKeywords()), KEYWORDS);

		Add(annotation, Table()
			.Add(AUTHORS, Urls(AUTHORS, dataProvider.GatAuthors()))
			.Add(SERIES, Url(SERIES, dataProvider.GatSeries().GetId(), dataProvider.GatSeries().GetRawData(NavigationItem::Column::Title)))
			.Add(GENRES, Urls(GENRES, dataProvider.GatGenres()))
			.Add(ARCHIVE, Url(ARCHIVE, dataProvider.GatBook().GetRawData(BookItem::Column::Folder), dataProvider.GatBook().GetRawData(BookItem::Column::Folder)))
			.ToString());

		Add(annotation, Table()
			.Add(FILENAME, dataProvider.GatBook().GetRawData(BookItem::Column::FileName))
			.Add(SIZE, QString("%L1").arg(dataProvider.GatBook().GetRawData(BookItem::Column::Size).toLongLong()))
			.Add(UPDATED, dataProvider.GatBook().GetRawData(BookItem::Column::UpdateDate))
			.ToString());
		m_ui.info->setText(annotation);
	}

private:
	AnnotationWidget & m_self;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<IAnnotationController, std::shared_ptr> m_annotationController;
	Ui::AnnotationWidget m_ui {};
};

AnnotationWidget::AnnotationWidget(std::shared_ptr<ISettings> settings
	, std::shared_ptr<IAnnotationController> annotationController
	, QWidget * parent
)
	: QWidget(parent)
	, m_impl(*this, std::move(settings), std::move(annotationController))
{
	PLOGD << "AnnotationWidget created";
}

AnnotationWidget::~AnnotationWidget()
{
	PLOGD << "AnnotationWidget destroyed";
}
