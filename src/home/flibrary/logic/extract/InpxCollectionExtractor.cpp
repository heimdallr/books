#include "InpxCollectionExtractor.h"

#include <filesystem>
#include <QBuffer>

#include <QCryptographicHash>
#include <QFileInfo>
#include <QTimer>

#include <plog/Log.h>

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"
#include "inpx/src/util/constant.h"
#include "Util/IExecutor.h"

#include "interface/constants/ExportStat.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IProgressController.h"

#include "data/DataItem.h"
#include "data/DataProvider.h"
#include "shared/ImageRestore.h"

#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

void Write(QByteArray & stream, const QString & uid, const BookInfo & book, size_t & n)
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
	stream.append((authors.join("") + FIELDS_SEPARATOR).toUtf8());

	for (const auto & genre : book.genres)
		stream.append((genre->GetRawData(GenreItem::Column::Fb2Code) + LIST_SEPARATOR).toUtf8());
	stream.append(QString(FIELDS_SEPARATOR).toUtf8());

	stream.append((book.book->GetRawData(BookItem::Column::Title) + FIELDS_SEPARATOR).toUtf8());
	stream.append((book.book->GetRawData(BookItem::Column::Series) + FIELDS_SEPARATOR).toUtf8());
	stream.append((seqNumber + FIELDS_SEPARATOR).toUtf8());
	stream.append((fileInfo.completeBaseName() + FIELDS_SEPARATOR).toUtf8());
	stream.append((book.book->GetRawData(BookItem::Column::Size) + FIELDS_SEPARATOR).toUtf8());
	stream.append((book.book->GetId() + FIELDS_SEPARATOR).toUtf8());
	stream.append((QString(book.book->To<BookItem>()->removed ? "1" : "0") + FIELDS_SEPARATOR).toUtf8());
	stream.append((fileInfo.suffix() + FIELDS_SEPARATOR).toUtf8());
	stream.append((book.book->GetRawData(BookItem::Column::UpdateDate) + FIELDS_SEPARATOR).toUtf8());
	stream.append((QString::number(n++) + FIELDS_SEPARATOR).toUtf8());
	stream.append((QString("%1.zip").arg(uid) + FIELDS_SEPARATOR).toUtf8());
	stream.append((book.book->GetRawData(BookItem::Column::Lang) + FIELDS_SEPARATOR + FIELDS_SEPARATOR).toUtf8());

	stream.append("\n");
}

QByteArray Process(const std::filesystem::path & archiveFolder
	, const QString & dstFolder
	, const QString & uid
	, const BookInfoList & books
	, IProgressController::IProgressItem & progress
)
{
	size_t n = 0;
	Zip zip(QString("%1/%2.zip").arg(dstFolder, uid), Zip::Format::Zip);
	QByteArray inpx;

	std::vector<QString> fileNames;
	fileNames.reserve(books.size());
	std::ranges::transform(books, std::back_inserter(fileNames), [] (const auto & book) { return book.book->GetRawData(BookItem::Column::FileName); });

	QByteArray bytes;

	zip.Write(fileNames, [&] (const size_t index) -> std::unique_ptr<QIODevice>
	{
		const auto & book = books[index];
		const auto fileName = book.book->GetRawData(BookItem::Column::FileName);
		const auto folder = QString::fromStdWString(archiveFolder / book.book->GetRawData(BookItem::Column::Folder).toStdWString());
		const Zip zipInput(folder);
		const auto input = zipInput.Read(fileName);
		bytes = RestoreImages(input->GetStream(), folder, fileName);
		progress.Increment(bytes.size());
		if (progress.IsStopped())
			return {};

		Write(inpx, uid, book, n);
		return std::make_unique<QBuffer>(&bytes);
	});

	return inpx;
}

}

class InpxCollectionExtractor::Impl final
{
public:
	Impl(std::shared_ptr<ICollectionController> collectionController
		, std::shared_ptr<IProgressController> progressController
		, std::shared_ptr<IDatabaseUser> databaseUser
		, const std::shared_ptr<const ILogicFactory>& logicFactory
	)
		: m_collectionController(std::move(collectionController))
		, m_progressController(std::move(progressController))
		, m_databaseUser(std::move(databaseUser))
		, m_logicFactory(logicFactory)
	{
	}

	void Extract(QString dstFolder, BookInfoList && books, Callback callback)
	{
		assert(!m_callback);
		m_hasError = false;
		m_callback = std::move(callback);
		m_taskCount = std::size(books) / 3000 + 1;
		ILogicFactory::Lock(m_logicFactory)->GetExecutor({ static_cast<int>(m_taskCount)}).swap(m_executor);
		m_dstFolder = std::move(dstFolder);
		m_archiveFolder = m_collectionController->GetActiveCollection().folder.toStdWString();

		CollectExistingFiles();

		std::vector<BookInfoList> bookLists(m_taskCount);
		for (size_t i = 0; auto && book : books)
			bookLists[i++ % m_taskCount].emplace_back(std::move(book));

		const auto transaction = m_databaseUser->Database()->CreateTransaction();
		const auto command = transaction->CreateCommand(ExportStat::INSERT_QUERY);

		std::for_each(std::make_move_iterator(std::begin(bookLists)), std::make_move_iterator(std::end(bookLists)), [&] (BookInfoList && bookInfoList)
		{
			for (const auto & book : bookInfoList)
			{
				command->Bind(0, book.book->GetId().toInt());
				command->Bind(1, static_cast<int>(ExportStat::Type::Inpx));
				command->Execute();
			}

			(*m_executor)(CreateTask(std::move(bookInfoList)));
		});

		transaction->Commit();
	}

	void GenerateInpx(QString inpxFileName, BookInfoList && books, Callback callback)
	{
		assert(!m_callback);
		m_hasError = false;
		m_callback = std::move(callback);
		ILogicFactory::Lock(m_logicFactory)->GetExecutor().swap(m_executor);
		std::shared_ptr progressItem = m_progressController->Add(static_cast<int64_t>(books.size()));

		(*m_executor)({ "Create inpx", [this, books = std::move(books), inpxFileName = std::move(inpxFileName), progressItem = std::move(progressItem), n= size_t{0}]() mutable
		{
			for (auto && book : books)
			{
				const auto id = QFileInfo(book.book->GetRawData(BookItem::Column::Folder)).completeBaseName();
				auto & stream = m_paths[id];
				Write(stream, id, book, n);
				progressItem->Increment(1);
				if (progressItem->IsStopped())
					break;
			}
			return [this, inpxFileName = std::move(inpxFileName)] (size_t)
			{
				WriteInpx(inpxFileName);
			};
		}});

	}

private:
	Util::IExecutor::Task CreateTask(BookInfoList && books)
	{
		const auto totalSize = std::accumulate(books.cbegin(), books.cend(), 0LL, [] (const size_t init, const auto & book) { return init + book.book->GetRawData(BookItem::Column::Size).toLongLong(); });
		std::shared_ptr progressItem = m_progressController->Add(totalSize);

		auto uid = [&]
		{
			QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
			hash.addData(QDateTime::currentDateTime().toString(Qt::ISODateWithMs).toUtf8());
			std::lock_guard lock(m_pathsGuard);
			for (auto i = m_paths.size(); ; ++i)
			{
				hash.addData(QString::number(i).toUtf8());
				auto result = hash.result().toHex().left(8);
				if (m_paths.try_emplace(result, QByteArray {}).second)
					return result;
			}
		}();

		auto taskName = uid.toStdString();
		return Util::IExecutor::Task
		{
			std::move(taskName), [this, books = std::move(books), progressItem = std::move(progressItem), uid = std::move(uid)] () mutable
			{
				bool error = false;
				QByteArray inpx;
				try
				{
					inpx = Process(m_archiveFolder, m_dstFolder, uid, books, *progressItem);
				}
				catch(const std::exception & ex)
				{
					PLOGE << ex.what();
					error = true;
				}
				return [this, error, inpx = std::move(inpx), uid = std::move(uid)] (const size_t) mutable
				{
					{
						std::lock_guard lock(m_pathsGuard);
						m_paths[uid] = std::move(inpx);
					}

					m_hasError = error || m_hasError;
					assert(m_taskCount > 0);
					if (--m_taskCount == 0)
						WriteInpx(GetInpxFileName());
				};
			}
		};
	}

	void CollectExistingFiles()
	{
		const auto inpxFileName = GetInpxFileName();
		if (!QFile::exists(inpxFileName))
			return;

		const Zip zip(inpxFileName);
		const auto files = zip.GetFileNameList();
		std::ranges::transform(files, std::inserter(m_paths, m_paths.end()), [] (const QString & file)
		{
			return std::make_pair(QFileInfo(file).completeBaseName(), QByteArray {});
		});
	}

	void WriteInpx(const QString & inpxFileName)
	{
		const auto inpxFileExists = QFile::exists(inpxFileName);

		std::vector<std::pair<QString, QByteArray>> toZip;

		if (!inpxFileExists)
		{
			toZip.emplace_back("collection.info", QString("Collection").toUtf8());
			toZip.emplace_back("version.info", QDateTime::currentDateTime().toString("yyyyMMdd").toUtf8());
		}

		for (auto && [uid, data] : m_paths)
			if (!data.isEmpty())
				toZip.emplace_back(QString("%1.inp").arg(uid), std::move(data));

		Zip zip(inpxFileName, Zip::Format::Zip, inpxFileExists);
		zip.Write(std::move(toZip));

		m_callback(m_hasError);
	}

	QString GetInpxFileName() const
	{
		return QString("%1/collection.inpx").arg(m_dstFolder);
	}

private:
	PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
	PropagateConstPtr<IProgressController, std::shared_ptr> m_progressController;
	std::shared_ptr<const IDatabaseUser> m_databaseUser;
	std::weak_ptr<const ILogicFactory> m_logicFactory;
	Callback m_callback;
	size_t m_taskCount { 0 };
	bool m_hasError { false };
	std::unique_ptr<Util::IExecutor> m_executor;
	QString m_dstFolder;
	std::filesystem::path m_archiveFolder;
	std::mutex m_pathsGuard;
	std::unordered_map<QString, QByteArray> m_paths;
};

InpxCollectionExtractor::InpxCollectionExtractor(std::shared_ptr<ICollectionController> collectionController
	, std::shared_ptr<IBooksExtractorProgressController> progressController
	, std::shared_ptr<IDatabaseUser> databaseUser
	, const std::shared_ptr<const ILogicFactory>& logicFactory
)
	: m_impl(std::move(collectionController)
		, std::move(progressController)
		, std::move(databaseUser)
		, logicFactory
	)
{
	PLOGV << "InpxCollectionExtractor created";
}

InpxCollectionExtractor::~InpxCollectionExtractor()
{
	PLOGV << "InpxCollectionExtractor destroyed";
}

void InpxCollectionExtractor::ExtractAsInpxCollection(QString folder, const std::vector<QString> & idList, const DataProvider & dataProvider, Callback callback)
{
	PLOGD << QString("Extract %1 books as inpx-collection started").arg(idList.size());

	std::vector<BookInfo> bookInfo;
	std::ranges::transform(idList, std::back_inserter(bookInfo), [&] (const auto & id) { return dataProvider.GetBookInfo(id.toLongLong()); });

	m_impl->Extract(std::move(folder), std::move(bookInfo), std::move(callback));
}

void InpxCollectionExtractor::GenerateInpx(QString inpxFileName, const std::vector<QString> & idList, const DataProvider & dataProvider, Callback callback)
{
	std::vector<BookInfo> bookInfo;
	std::ranges::transform(idList, std::back_inserter(bookInfo), [&] (const auto & id) { return dataProvider.GetBookInfo(id.toLongLong()); });

	m_impl->GenerateInpx(std::move(inpxFileName), std::move(bookInfo), std::move(callback));
}
