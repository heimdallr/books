#include "ArchiveParser.h"

#include <QFile>

#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "interface/constants/Localization.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/IProgressController.h"

#include "data/DataItem.h"
#include "shared/ImageRestore.h"
#include "shared/ZipProgressCallback.h"
#include "util/SaxParser.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto CONTEXT = "Annotation";
constexpr auto CONTENT = QT_TRANSLATE_NOOP("Annotation", "Content");
constexpr auto FILE_EMPTY = QT_TRANSLATE_NOOP("Annotation", "File is empty");
TR_DEF

constexpr auto ID = "id";
constexpr auto L_HREF = "l:href";
constexpr auto A = "a";
constexpr auto ANNOTATION = "FictionBook/description/title-info/annotation";
constexpr auto ANNOTATION_P = "FictionBook/description/title-info/annotation/p";
constexpr auto ANNOTATION_HREF = "FictionBook/description/title-info/annotation/a";
constexpr auto ANNOTATION_HREF_P = "FictionBook/description/title-info/annotation/p/a";
constexpr auto KEYWORDS = "FictionBook/description/title-info/keywords";
constexpr auto BINARY = "FictionBook/binary";
constexpr auto COVERPAGE_IMAGE = "FictionBook/description/title-info/coverpage/image";
constexpr auto SECTION = "section";
constexpr auto SECTION_TITLE = "section/title";
constexpr auto SECTION_TITLE_P = "section/title/p";
constexpr auto SECTION_TITLE_P_STRONG = "section/title/p/strong";
constexpr auto EPIGRAPH = "FictionBook/body/epigraph";
constexpr auto EPIGRAPH_P = "FictionBook/body/epigraph/p";
constexpr auto EPIGRAPH_AUTHOR = "FictionBook/body/epigraph/text-author";

class XmlParser final
	: public Util::SaxParser
{
public:
	explicit XmlParser(QIODevice & ioDevice)
		: SaxParser(ioDevice)
		, m_ioDevice(ioDevice)
		, m_total(m_ioDevice.size())
	{
		m_data.content->SetData(Tr(CONTENT), NavigationItem::Column::Title);
	}

	ArchiveParser::Data Parse(const QString & rootFolder, const IDataItem & book, std::unique_ptr<IProgressController::IProgressItem> progressItem)
	{
		if (m_total == 0)
		{
			m_data.error = Tr(FILE_EMPTY);
			PLOGE << m_data.error;
			return m_data;
		}

		m_progressItem = std::move(progressItem);

		SaxParser::Parse();

		update_covers(rootFolder, book);

		for (auto&& [name, bytes] : m_covers)
		{
			if (bytes.isNull())
				continue;

			if (name == m_coverpage)
				m_data.coverIndex = static_cast<int>(m_data.covers.size());
			m_data.covers.push_back(std::move(bytes));
		}

		return m_data;
	}

private: // Util::SaxParser
	bool OnStartElement(const QString & name, const QString & path, const Attributes & attributes) override
	{
		if (name.compare(A, Qt::CaseInsensitive) == 0)
			m_href = attributes.GetAttribute(L_HREF);

		using ParseElementFunction = bool(XmlParser::*)(const Attributes &);
		using ParseElementItem = std::pair<const char *, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[]
		{
			{ COVERPAGE_IMAGE, &XmlParser::OnStartElementCoverpageImage },
			{ BINARY, &XmlParser::OnStartElementBinary },
			{ SECTION, &XmlParser::OnStartElementSection },
		};

		return SaxParser::Parse(*this, PARSERS, path, attributes);
	}

	bool OnEndElement(const QString & path) override
	{
		const auto percents = std::lround(100 * m_ioDevice.pos() / m_total);
		if (m_percents < percents)
		{
			m_progressItem->Increment(percents - m_percents);
			m_percents = percents;
		}

		using ParseElementFunction = bool(XmlParser::*)();
		using ParseElementItem = std::pair<const char *, ParseElementFunction>;
		static constexpr ParseElementItem PARSERS[]
		{
			{ SECTION, &XmlParser::OnEndElementSection },
		};

		return SaxParser::Parse(*this, PARSERS, path);
	}

	bool OnCharacters(const QString & path, const QString & value) override
	{
		using ParseCharacterFunction = bool(XmlParser::*)(const QString &);
		using ParseCharacterItem = std::pair<const char *, ParseCharacterFunction>;
		static constexpr ParseCharacterItem PARSERS[]
		{
			{ ANNOTATION            , &XmlParser::ParseAnnotation },
			{ ANNOTATION_P          , &XmlParser::ParseAnnotation },
			{ ANNOTATION_HREF       , &XmlParser::ParseAnnotationHref },
			{ ANNOTATION_HREF_P     , &XmlParser::ParseAnnotationHref },
			{ KEYWORDS              , &XmlParser::ParseKeywords },
			{ BINARY                , &XmlParser::ParseBinary },
			{ SECTION_TITLE         , &XmlParser::ParseSectionTitle },
			{ SECTION_TITLE_P       , &XmlParser::ParseSectionTitle },
			{ SECTION_TITLE_P_STRONG, &XmlParser::ParseSectionTitle },
			{ EPIGRAPH              , &XmlParser::ParseEpigraph },
			{ EPIGRAPH_P            , &XmlParser::ParseEpigraph },
			{ EPIGRAPH_AUTHOR       , &XmlParser::ParseEpigraphAuthor },
		};

		return SaxParser::Parse(*this, PARSERS, path, value);
	}

	bool OnWarning(const QString & text) override
	{
		m_data.error = text;
		PLOGW << text;
		return true;
	}

	bool OnError(const QString & text) override
	{
		return OnWarning(text);
	}

	bool OnFatalError(const QString & text) override
	{
		return OnWarning(text);
	}

private:
	bool OnStartElementCoverpageImage(const Attributes & attributes)
	{
		m_coverpage = attributes.GetAttribute(L_HREF);
		while (!m_coverpage.isEmpty() && m_coverpage.front() == '#')
			m_coverpage.removeFirst();

		return true;
	}

	bool OnStartElementBinary(const Attributes & attributes)
	{
		m_covers.emplace_back(attributes.GetAttribute(ID), QByteArray {});
		return true;
	}

	bool OnStartElementSection(const Attributes &)
	{
		m_currentContentItem = m_currentContentItem->AppendChild(NavigationItem::Create()).get();
		return true;
	}

	bool OnEndElementSection()
	{
		const auto remove = m_currentContentItem->GetData(NavigationItem::Column::Title).isEmpty();
		m_currentContentItem = m_currentContentItem->GetParent();
		if (remove)
			m_currentContentItem->RemoveChild();

		return true;
	}

	bool ParseAnnotation(const QString & value)
	{
		m_data.annotation.append(QString("<p>%1</p>").arg(value));
		return true;
	}

	bool ParseAnnotationHref(const QString & value)
	{
		m_data.annotation.append(QString(R"(<p><a href="%1">%2</a></p>)").arg(m_href).arg(value));
		return true;
	}

	bool ParseKeywords(const QString & value)
	{
		std::ranges::copy(value.split(",", Qt::SkipEmptyParts), std::back_inserter(m_data.keywords));
		return true;
	}

	bool ParseBinary(const QString & value)
	{
		m_covers.back().second = QByteArray::fromBase64(value.toUtf8());
		return true;
	}

	bool ParseSectionTitle(const QString & value)
	{
		if (std::ranges::all_of(value, [] (auto c) { return c.isDigit(); }))
			return true;

		auto currentValue = m_currentContentItem->GetData(NavigationItem::Column::Title);
		m_currentContentItem->SetData(currentValue.append(currentValue.isEmpty() ? "" : ". ").append(value), NavigationItem::Column::Title);
		return true;
	}

	bool ParseEpigraph(const QString & value)
	{
		m_data.epigraph = value;
		return true;
	}

	bool ParseEpigraphAuthor(const QString & value)
	{
		m_data.epigraphAuthor = value;
		return true;
	}

	void update_covers(const QString & rootFolder, const IDataItem & book)
	{
		if (std::ranges::none_of(m_covers, [] (const auto & item)
		{
			return item.second.isNull();
		}))
			return;

		const auto zip = CreateImageArchive(QString("%1/%2").arg(rootFolder, book.GetRawData(BookItem::Column::Folder)));
		if (!zip)
			return;

		const auto read = [&](const QString &id)
		{
			try
			{
				return zip->Read(QString("%1/%2").arg(book.GetRawData(BookItem::Column::FileName), id)).readAll();
			}
			catch(const std::exception & ex)
			{
				PLOGE << ex.what();
			}
			return QByteArray {};
		};

		for (auto & [id, bytes] : m_covers)
			if (bytes.isNull())
				bytes = read(id);
	}

private:
	QIODevice & m_ioDevice;
	int64_t m_total;
	std::unique_ptr<IProgressController::IProgressItem> m_progressItem;

	ArchiveParser::Data m_data;
	QString m_href;
	QString m_coverpage;
	IDataItem * m_currentContentItem { m_data.content.get() };
	std::vector<std::pair<QString, QByteArray>> m_covers;
	int64_t m_percents { 0 };
};

}

class ArchiveParser::Impl
	: virtual ZipProgressCallback::IObserver
	, virtual IProgressController::IObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	explicit Impl(std::shared_ptr<ICollectionController> collectionController
		, std::shared_ptr<IProgressController> progressController
		, std::shared_ptr<ZipProgressCallback> zipProgressCallback
	)
		: m_collectionController(std::move(collectionController))
		, m_progressController(std::move(progressController))
		, m_zipProgressCallback(std::move(zipProgressCallback))
	{
		m_zipProgressCallback->RegisterObserver(this);
		m_progressController->RegisterObserver(this);
	}

	~Impl() override
	{
		m_progressController->UnregisterObserver(this);
		m_zipProgressCallback->UnregisterObserver(this);
	}

	[[nodiscard]] std::shared_ptr<IProgressController> GetProgressController() const
	{
		return m_progressController;
	}

	[[nodiscard]] Data Parse(const IDataItem & book) const
	{
		const auto collection = m_collectionController->GetActiveCollection();
		assert(collection);
		const auto folder = QString("%1/%2").arg(collection->folder, book.GetRawData(BookItem::Column::Folder));
		if (!QFile::exists(folder))
		{
			PLOGW << folder << " not found";
			return {};
		}

		try
		{
			m_extractArchivePercents = 0;
			m_extractArchiveProgressItem = m_progressController->Add(100);
			auto parseProgressItem = m_progressController->Add(100);

			const Zip zip(folder, m_zipProgressCallback);
			auto & stream = zip.Read(book.GetRawData(BookItem::Column::FileName));
			m_extractArchiveProgressItem.reset();

			XmlParser parser(stream);
			return parser.Parse(collection->folder, book, std::move(parseProgressItem));
		}
		catch(...) {}
		return {};
	}

private: // ZipProgressCallback::IObserver
	void OnProgress(const int percents) override
	{
		if (m_extractArchiveProgressItem)
			m_extractArchiveProgressItem->Increment(percents - m_extractArchivePercents);
		m_extractArchivePercents = percents;
	}

private: // IProgressController::IObserver
	void OnStartedChanged() override
	{
	}

	void OnValueChanged() override
	{
	}

	void OnStop() override
	{
		m_zipProgressCallback->Stop();
	}

private:
	PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
	std::shared_ptr<IProgressController> m_progressController;
	std::shared_ptr<ZipProgressCallback> m_zipProgressCallback;
	mutable std::unique_ptr<IProgressController::IProgressItem> m_extractArchiveProgressItem;
	mutable int m_extractArchivePercents { 0 };
};

ArchiveParser::ArchiveParser(std::shared_ptr<ICollectionController> collectionController
	, std::shared_ptr<IAnnotationProgressController> progressController
	, std::shared_ptr<ZipProgressCallback> zipProgressCallback
)
	: m_impl(std::move(collectionController)
		, std::move(progressController)
		, std::move(zipProgressCallback)
	)
{
	PLOGD << "AnnotationParser created";
}

ArchiveParser::~ArchiveParser()
{
	PLOGD << "AnnotationParser destroyed";
}

std::shared_ptr<IProgressController> ArchiveParser::GetProgressController() const
{
	return m_impl->GetProgressController();
}

ArchiveParser::Data  ArchiveParser::Parse(const IDataItem & dataItem) const
{
	return m_impl->Parse(dataItem);
}
