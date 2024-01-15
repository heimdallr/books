#include "InpxCollectionExtractor.h"

#include <filesystem>

#include <QTemporaryDir>
#include <QTimer>

#include <plog/Log.h>

#include "Util/IExecutor.h"
#include "inpx/src/util/constant.h"

#include "interface/logic/ICollectionController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IProgressController.h"

#include "data/DataItem.h"
#include "data/DataProvider.h"
#include "shared/ImageRestore.h"

#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

void Write(QIODevice & stream, const BookInfo & book, size_t & n)
{
	auto seqNumber = book.book->GetRawData(BookItem::Column::SeqNumber);
	if (seqNumber.toInt() <= 0)
		seqNumber = "0";

	const QFileInfo fileInfo(book.book->GetRawData(BookItem::Column::FileName));

	QStringList authors;
	for (const auto & author : book.authors)
	{
		const QStringList authorNames = QStringList()
			<< author->GetRawData(AuthorItem::Column::LastName)
			<< author->GetRawData(AuthorItem::Column::FirstName)
			<< author->GetRawData(AuthorItem::Column::MiddleName)
			;
		authors << authorNames.join(NAMES_SEPARATOR) << QString(LIST_SEPARATOR);
	}
	stream.write((authors.join("") + FIELDS_SEPARATOR).toUtf8());

	for (const auto & genre : book.genres)
		stream.write((genre->GetRawData(GenreItem::Column::Fb2Code) + LIST_SEPARATOR).toUtf8());
	stream.write(QString(FIELDS_SEPARATOR).toUtf8());

	stream.write((book.book->GetRawData(BookItem::Column::Title) + FIELDS_SEPARATOR).toUtf8());
	stream.write((book.book->GetRawData(BookItem::Column::Series) + FIELDS_SEPARATOR).toUtf8());
	stream.write((seqNumber + FIELDS_SEPARATOR).toUtf8());
	stream.write((fileInfo.completeBaseName() + FIELDS_SEPARATOR).toUtf8());
	stream.write((book.book->GetRawData(BookItem::Column::Size) + FIELDS_SEPARATOR).toUtf8());
	stream.write((book.book->GetId() + FIELDS_SEPARATOR).toUtf8());
	stream.write((QString(book.book->To<BookItem>()->removed ? "1" : "0") + FIELDS_SEPARATOR).toUtf8());
	stream.write((fileInfo.suffix() + FIELDS_SEPARATOR).toUtf8());
	stream.write((book.book->GetRawData(BookItem::Column::UpdateDate) + FIELDS_SEPARATOR).toUtf8());
	stream.write((QString::number(n++) + FIELDS_SEPARATOR).toUtf8());
	stream.write((book.book->GetRawData(BookItem::Column::Folder) + FIELDS_SEPARATOR).toUtf8());
	stream.write((book.book->GetRawData(BookItem::Column::Lang) + FIELDS_SEPARATOR + FIELDS_SEPARATOR).toUtf8());

	stream.write("\n");
}

void Process(const std::filesystem::path & archiveFolder
	, const BookInfo & book
	, IProgressController::IProgressItem & progress
	, Zip & zipOutput
	, QIODevice & inpx
	, size_t & n
)
{
	const auto fileName = book.book->GetRawData(BookItem::Column::FileName);
	const auto folder = QString::fromStdWString(archiveFolder / book.book->GetRawData(BookItem::Column::Folder).toStdWString());
	auto & output = zipOutput.Write(fileName);
	const Zip zipInput(folder);
	auto & input = zipInput.Read(fileName);
	const auto bytes = RestoreImages(input, folder, fileName);
	output.write(bytes);
	Write(inpx, book, n);
	progress.Increment(input.size());
}

using ProcessFunctor = std::function<void(const std::filesystem::path & archiveFolder
	, const QString & dstFolder
	, const BookInfo & book
	, IProgressController::IProgressItem & progress
	)>;
}

class InpxCollectionExtractor::Impl final
	: IProgressController::IObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(std::shared_ptr<ICollectionController> collectionController
		, std::shared_ptr<IProgressController> progressController
		, std::shared_ptr<ILogicFactory> logicFactory
	)
		: m_collectionController(std::move(collectionController))
		, m_progressController(std::move(progressController))
		, m_logicFactory(std::move(logicFactory))
	{
		m_progressController->RegisterObserver(this);
	}

	~Impl() override
	{
		m_progressController->UnregisterObserver(this);
	}

	void Extract(QString dstFolder, BookInfoList && books, Callback callback, ProcessFunctor processFunctor, const int maxThreadCount = std::numeric_limits<int>::max())
	{
		assert(!m_callback);
		m_hasError = false;
		m_callback = std::move(callback);
		m_taskCount = std::size(books);
		m_processFunctor = std::move(processFunctor);
		m_logicFactory->GetExecutor({ std::min(static_cast<int>(m_taskCount), maxThreadCount)}).swap(m_executor);
		m_dstFolder = std::move(dstFolder);
		m_archiveFolder = m_collectionController->GetActiveCollection()->folder.toStdWString();

		std::for_each(std::make_move_iterator(std::begin(books)), std::make_move_iterator(std::end(books)), [&] (BookInfo && book)
		{
			(*m_executor)(CreateTask(std::move(book)));
		});
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
		m_executor->Stop();
		m_executor.reset();
		QTimer::singleShot(0, [&]
		{
			m_callback(m_hasError);
		});
	}

private:
	Util::IExecutor::Task CreateTask(BookInfo && book)
	{
		std::shared_ptr progressItem = m_progressController->Add(book.book->GetRawData(BookItem::Column::Size).toLongLong());
		auto taskName = book.book->GetRawData(BookItem::Column::FileName).toStdString();
		return Util::IExecutor::Task
		{
			std::move(taskName), [&, book = std::move(book), progressItem = std::move(progressItem)] ()
			{
				bool error = false;
				try
				{
					m_processFunctor(m_archiveFolder, m_dstFolder, book, *progressItem);
				}
				catch(const std::exception & ex)
				{
					PLOGE << ex.what();
					error = true;
				}
				return [&, error] (const size_t)
				{
					m_hasError = error || m_hasError;
					assert(m_taskCount > 0);
					if (--m_taskCount == 0)
						m_callback(m_hasError);
				};
			}
		};
	}

private:
	PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
	PropagateConstPtr<IProgressController, std::shared_ptr> m_progressController;
	PropagateConstPtr<ILogicFactory, std::shared_ptr> m_logicFactory;
	Callback m_callback;
	size_t m_taskCount { 0 };
	bool m_hasError { false };
	ProcessFunctor m_processFunctor { nullptr };
	std::unique_ptr<Util::IExecutor> m_executor;
	QString m_dstFolder;
	std::filesystem::path m_archiveFolder;
	std::mutex m_usedPathGuard;
	std::unordered_set<QString> m_usedPath;
};

InpxCollectionExtractor::InpxCollectionExtractor(std::shared_ptr<ICollectionController> collectionController
	, std::shared_ptr<IBooksExtractorProgressController> progressController
	, std::shared_ptr<ILogicFactory> logicFactory
)
	: m_impl(std::move(collectionController)
		, std::move(progressController)
		, std::move(logicFactory)
	)
{
	PLOGD << "InpxCollectionExtractor created";
}

InpxCollectionExtractor::~InpxCollectionExtractor()
{
	PLOGD << "InpxCollectionExtractor destroyed";
}

void InpxCollectionExtractor::ExtractAsInpxCollection(QString folder, const std::vector<QString> & idList, const DataProvider & dataProvider, Callback callback)
{
	PLOGD << QString("Extract %1 books as inpx-collection started").arg(idList.size());

	std::vector<BookInfo> bookInfo;
	std::ranges::transform(idList, std::back_inserter(bookInfo), [&] (const auto & id) { return dataProvider.GetBookInfo(id.toLongLong()); });
	const auto archiveName = QDateTime::currentDateTime().toString("yyyyMMddhhmmss");
	auto zip = std::make_shared<Zip>(QString("%1/%2.zip").arg(folder, archiveName), Zip::Format::Zip);
	const auto inpxFileName = QString("%1/collection.inpx").arg(folder);
	const auto inpxFileExists = QFile::exists(inpxFileName);
	auto inpx = std::make_shared<Zip>(inpxFileName, Zip::Format::Zip, inpxFileExists);
	if (!inpxFileExists)
	{
		{
			auto & stream = inpx->Write("collection.info");
			stream.write("Collection");
		}
		{
			auto & stream = inpx->Write("version.info");
			stream.write(archiveName.left(8).toUtf8());
		}
	}

	auto & inpxStream = inpx->Write(QString("%1.inp").arg(archiveName));
	m_impl->Extract(std::move(folder), std::move(bookInfo), std::move(callback), [zip = std::move(zip), inpx = std::move(inpx), &inpxStream, n = size_t{0}]
		(const std::filesystem::path & archiveFolder
			, const QString & /*dstFolder*/
			, const BookInfo & book
			, IProgressController::IProgressItem & progress
			) mutable
	{
		Process(archiveFolder, book, progress, *zip, inpxStream, n);
	}, 1);
}

auto create()
{
	return new InpxCollectionExtractor(std::shared_ptr<ICollectionController>{}, std::shared_ptr<IBooksExtractorProgressController>{}, std::shared_ptr<ILogicFactory>{});
}