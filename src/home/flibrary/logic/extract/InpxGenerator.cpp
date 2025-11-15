#include "InpxGenerator.h"

#include <QCryptographicHash>

#include <filesystem>

#include <QFileInfo>
#include <QTimer>

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/ExportStat.h"
#include "interface/logic/IDataProvider.h"

#include "Util/IExecutor.h"
#include "data/DataItem.h"
#include "shared/ImageRestore.h"
#include "util/Fb2InpxParser.h"

#include "log.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
using Genres = std::unordered_map<QString, QString>;

Genres GetGenres(DB::IDatabase& db)
{
	Genres result;

	const auto query = db.CreateQuery("select GenreCode, FB2Code, ParentCode from Genres");
	for (query->Execute(); !query->Eof(); query->Next())
		const auto& [code, name] = *result.try_emplace(query->Get<const char*>(0), query->Get<const char*>(1)).first;

	return result;
}

using BookGenres = std::unordered_map<long long, std::vector<QString>>;

BookGenres GetBookGenres(DB::IDatabase& db, const Genres& genres)
{
	BookGenres result;
	const auto query = db.CreateQuery("select BookID, GenreCode from Genre_List order by BookID, GenreCode");
	for (query->Execute(); !query->Eof(); query->Next())
		if (auto genre = QString(query->Get<const char*>(1)); genres.contains(genre))
			result[query->Get<long long>(0)].emplace_back(std::move(genre));

	return result;
}

using Author  = std::tuple<QString, QString, QString>;
using Authors = std::unordered_map<long long, Author>;

Authors GetAuthors(DB::IDatabase& db)
{
	Authors    result;
	const auto query = db.CreateQuery("select AuthorID, LastName, FirstName, MiddleName from Authors");
	for (query->Execute(); !query->Eof(); query->Next())
		result.try_emplace(query->Get<long long>(0), Author { query->Get<const char*>(1), query->Get<const char*>(2), query->Get<const char*>(3) });

	return result;
}

using BookAuthors = std::unordered_map<long long, std::vector<long long>>;

BookAuthors GetBookAuthors(DB::IDatabase& db, const Authors& authors)
{
	BookAuthors result;
	const auto  query = db.CreateQuery("select BookID, AuthorID from Author_List order by BookID, AuthorID");
	for (query->Execute(); !query->Eof(); query->Next())
		if (const auto id = query->Get<long long>(1); authors.contains(id))
			result[query->Get<long long>(0)].emplace_back(id);

	return result;
}

using Series = std::unordered_map<long long, QString>;

Series GetSeries(DB::IDatabase& db)
{
	Series     result;
	const auto query = db.CreateQuery("select SeriesID, SeriesTitle from Series");
	for (query->Execute(); !query->Eof(); query->Next())
		result.try_emplace(query->Get<long long>(0), query->Get<const char*>(1));

	return result;
}

using Keywords = std::unordered_map<long long, QString>;

Keywords GetKeywords(DB::IDatabase& db)
{
	Keywords   result;
	const auto query = db.CreateQuery("select KeywordID, KeywordTitle from Keywords");
	for (query->Execute(); !query->Eof(); query->Next())
		result.try_emplace(query->Get<long long>(0), query->Get<const char*>(1));

	return result;
}

using BookKeywords = std::unordered_map<long long, std::vector<long long>>;

BookKeywords GetBookKeywords(DB::IDatabase& db, const Keywords& keywords)
{
	BookKeywords result;
	const auto   query = db.CreateQuery("select BookID, KeywordID from Keyword_List");
	for (query->Execute(); !query->Eof(); query->Next())
		if (const auto id = query->Get<long long>(1); keywords.contains(id))
			result[query->Get<long long>(0)].emplace_back(id);

	return result;
}

//"AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;LANG;LIBRATE;KEYWORDS;"

void Write(QByteArray& stream, const QString& uid, const BookInfo& book, size_t& n)
{
	auto seqNumber = book.book->GetRawData(BookItem::Column::SeqNumber);
	if (seqNumber.toInt() <= 0)
		seqNumber = "0";

	const QFileInfo fileInfo(book.book->GetRawData(BookItem::Column::FileName));

	QStringList authors;
	for (const auto& author : book.authors)
	{
		const QStringList authorNames = QStringList() << author->GetRawData(AuthorItem::Column::LastName) << author->GetRawData(AuthorItem::Column::FirstName)
		                                              << author->GetRawData(AuthorItem::Column::MiddleName);
		authors << authorNames.join(Util::Fb2InpxParser::NAMES_SEPARATOR) << QString(Util::Fb2InpxParser::LIST_SEPARATOR);
	}
	stream.append((authors.join("") + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());

	for (const auto& genre : book.genres)
		stream.append((genre->GetRawData(GenreItem::Column::Fb2Code) + Util::Fb2InpxParser::LIST_SEPARATOR).toUtf8());
	stream.append(QString(Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());

	stream.append((book.book->GetRawData(BookItem::Column::Title) + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((book.book->GetRawData(BookItem::Column::Series) + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((seqNumber + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((fileInfo.completeBaseName() + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((book.book->GetRawData(BookItem::Column::Size) + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((book.book->GetId() + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((QString(book.book->IsRemoved() ? "1" : "0") + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((fileInfo.suffix() + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((book.book->GetRawData(BookItem::Column::UpdateDate) + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((QString::number(n++) + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((QString("%1.zip").arg(uid) + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((book.book->GetRawData(BookItem::Column::Lang) + Util::Fb2InpxParser::FIELDS_SEPARATOR + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());

	stream.append("\n");
}

constexpr auto BOOK_QUERY = R"(
select
b.BookID, b.LibID, b.Title, sl.SeriesID, sl.SeqNumber, b.UpdateDate, b.LibRate, b.Lang, f.FolderTitle, b.FileName, b.InsideNo, b.Ext, b.BookSize, coalesce(bu.IsDeleted, b.IsDeleted)
from Books b
join Folders f on f.FolderID = b.FolderID
left join Books_User bu on bu.BookID = b.BookID
left join Series_List sl on sl.BookID = b.BookID
order by f.FolderID, b.InsideNo
)";

void Write(
	QByteArray&         stream,
	const DB::IQuery&   query,
	const Series&       series,
	const Genres&       genres,
	const BookGenres&   bookGenres,
	const Authors&      authors,
	const BookAuthors&  bookAuthors,
	const Keywords&     keywords,
	const BookKeywords& bookKeywords
)
{
	const auto bookId = query.Get<long long>(0);

	QString seriesTitle;
	if (const auto it = series.find(query.Get<long long>(3)); it != series.end())
		seriesTitle = it->second;

	const auto getList = [bookId](const auto& dictionary, const auto& index, const auto f) -> QStringList {
		QStringList result;
		if (const auto it = index.find(bookId); it != index.end())
			std::ranges::transform(it->second, std::back_inserter(result), [&](const auto& id) {
				const auto itemIt = dictionary.find(id);
				assert(itemIt != dictionary.end());
				return f(itemIt->second) + Util::Fb2InpxParser::LIST_SEPARATOR;
			});
		return result;
	};

	const auto genreList   = getList(genres, bookGenres, [](const auto& item) {
        return item;
    });
	const auto keywordList = getList(keywords, bookKeywords, [](const auto& item) {
		return item;
	});
	const auto authorList  = getList(authors, bookAuthors, [](const auto& item) {
        const auto& [lastName, firstName, middleName] = item;
        return (QStringList() << lastName << firstName << middleName).join(Util::Fb2InpxParser::NAMES_SEPARATOR);
    });

	auto book = QStringList {} << authorList.join("") << genreList.join("") << query.Get<const char*>(2) << seriesTitle << Util::Fb2InpxParser::GetSeqNumber(query.Get<const char*>(4))
	                           << query.Get<const char*>(9) << QString::number(query.Get<long long>(12)) << query.Get<const char*>(1) << QString::number(query.Get<int>(13)) << query.Get<const char*>(11) + 1
	                           << query.Get<const char*>(5) << query.Get<const char*>(7) << Util::Fb2InpxParser::GetSeqNumber(query.Get<const char*>(6)) << keywordList.join("");

	stream.append(book.join(Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8()).append(Util::Fb2InpxParser::FIELDS_SEPARATOR).append("\r\n");
}

QByteArray Process(const std::filesystem::path& archiveFolder, const QString& dstFolder, const QString& uid, const BookInfoList& books, IProgressController::IProgressItem& progress)
{
	size_t     n = 0;
	QByteArray inpx;

	for (const auto& book : books)
	{
		const auto fileName = book.book->GetRawData(BookItem::Column::FileName);
		const auto folder   = QString::fromStdWString(archiveFolder / book.book->GetRawData(BookItem::Column::Folder).toStdWString());
		const Zip  zipInput(folder);
		const auto input = zipInput.Read(fileName);
		auto       bytes = PrepareToExport(input->GetStream(), folder, fileName);
		progress.Increment(bytes.size());
		Write(inpx, uid, book, n);

		auto zipFiles = Zip::CreateZipFileController();
		zipFiles->AddFile(book.book->GetRawData(BookItem::Column::FileName), bytes, QDateTime::currentDateTime());
		Zip zip(QString("%1/%2.zip").arg(dstFolder, uid), Zip::Format::Zip, true);
		zip.Write(std::move(zipFiles));
	}

	return inpx;
}

} // namespace

class InpxGenerator::Impl final
{
public:
	Impl(
		const std::shared_ptr<const ILogicFactory>& logicFactory,
		std::shared_ptr<const ICollectionProvider>  collectionProvider,
		std::shared_ptr<const IDatabaseUser>        databaseUser,
		std::shared_ptr<IProgressController>        progressController
	)
		: m_logicFactory(logicFactory)
		, m_collectionProvider(std::move(collectionProvider))
		, m_databaseUser(std::move(databaseUser))
		, m_progressController(std::move(progressController))
	{
	}

	void Extract(QString dstFolder, BookInfoList&& books, Callback callback)
	{
		assert(!m_callback);
		m_hasError  = false;
		m_callback  = std::move(callback);
		m_taskCount = std::size(books) / 3000 + 1;
		ILogicFactory::Lock(m_logicFactory)->GetExecutor({ static_cast<int>(m_taskCount) }).swap(m_executor);
		m_dstFolder     = std::move(dstFolder);
		m_archiveFolder = m_collectionProvider->GetActiveCollection().GetFolder().toStdWString();

		CollectExistingFiles();

		std::vector<BookInfoList> bookLists(m_taskCount);
		for (size_t i = 0; auto&& book : books)
			bookLists[i++ % m_taskCount].emplace_back(std::move(book));

		const auto transaction = m_databaseUser->Database()->CreateTransaction();
		const auto command     = transaction->CreateCommand(ExportStat::INSERT_QUERY);

		std::for_each(std::make_move_iterator(std::begin(bookLists)), std::make_move_iterator(std::end(bookLists)), [&](BookInfoList&& bookInfoList) {
			for (const auto& book : bookInfoList)
			{
				command->Bind(0, book.book->GetId().toInt());
				command->Bind(1, static_cast<int>(ExportStat::Type::Inpx));
				command->Execute();
			}

			(*m_executor)(CreateTask(std::move(bookInfoList)));
		});

		transaction->Commit();
	}

	void GenerateInpx(QString inpxFileName, BookInfoList&& books, Callback callback)
	{
		assert(!m_callback);
		m_hasError = false;
		m_callback = std::move(callback);
		ILogicFactory::Lock(m_logicFactory)->GetExecutor().swap(m_executor);
		std::shared_ptr progressItem = m_progressController->Add(static_cast<int64_t>(books.size()));

		(*m_executor)({ "Create inpx", [this, books = std::move(books), inpxFileName = std::move(inpxFileName), progressItem = std::move(progressItem), n = size_t { 0 }]() mutable {
						   for (auto&& book : books)
						   {
							   const auto id     = QFileInfo(book.book->GetRawData(BookItem::Column::Folder)).completeBaseName();
							   auto&      stream = m_paths[id];
							   Write(stream, id, book, n);
							   progressItem->Increment(1);
							   if (progressItem->IsStopped())
								   break;
						   }
						   return [this, inpxFileName = std::move(inpxFileName)](size_t) {
							   WriteInpx(inpxFileName);
						   };
					   } });
	}

	void GenerateInpx(QString inpxFileName, Callback callback)
	{
		assert(!m_callback);
		m_hasError = false;
		ILogicFactory::Lock(m_logicFactory)->GetExecutor().swap(m_executor);
		(*m_executor)({ "Create inpx", [this, inpxFileName = std::move(inpxFileName), callback = std::move(callback)]() mutable {
						   GenerateInpxImpl(inpxFileName);
						   return [this, callback = std::move(callback)](size_t) {
							   callback(!m_hasError);
						   };
					   } });
	}

private:
	Util::IExecutor::Task CreateTask(BookInfoList&& books)
	{
		const auto      totalSize    = std::accumulate(books.cbegin(), books.cend(), 0LL, [](const size_t init, const auto& book) {
            return init + book.book->GetRawData(BookItem::Column::Size).toLongLong();
        });
		std::shared_ptr progressItem = m_progressController->Add(totalSize);

		auto uid = [&] {
			QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
			hash.addData(QDateTime::currentDateTime().toString(Qt::ISODateWithMs).toUtf8());
			std::lock_guard lock(m_pathsGuard);
			for (auto i = m_paths.size();; ++i)
			{
				hash.addData(QString::number(i).toUtf8());
				auto result = hash.result().toHex().left(8);
				if (m_paths.try_emplace(result, QByteArray {}).second)
					return result;
			}
		}();

		auto taskName = uid.toStdString();
		return Util::IExecutor::Task { std::move(taskName), [this, books = std::move(books), progressItem = std::move(progressItem), uid = std::move(uid)]() mutable {
										  bool       error = false;
										  QByteArray inpx;
										  try
										  {
											  inpx = Process(m_archiveFolder, m_dstFolder, uid, books, *progressItem);
										  }
										  catch (const std::exception& ex)
										  {
											  PLOGE << ex.what();
											  error = true;
										  }
										  return [this, error, inpx = std::move(inpx), uid = std::move(uid)](const size_t) mutable {
											  {
												  std::lock_guard lock(m_pathsGuard);
												  m_paths[uid] = std::move(inpx);
											  }

											  m_hasError = error || m_hasError;
											  assert(m_taskCount > 0);
											  if (--m_taskCount == 0)
												  WriteInpx(GetInpxFileName());
										  };
									  } };
	}

	void CollectExistingFiles()
	{
		const auto inpxFileName = GetInpxFileName();
		if (!QFile::exists(inpxFileName))
			return;

		const Zip  zip(inpxFileName);
		const auto files = zip.GetFileNameList();
		std::ranges::transform(files, std::inserter(m_paths, m_paths.end()), [](const QString& file) {
			return std::make_pair(QFileInfo(file).completeBaseName(), QByteArray {});
		});
	}

	void WriteInpx(const QString& inpxFileName)
	{
		const auto inpxFileExists = QFile::exists(inpxFileName);

		auto zipFileController = Zip::CreateZipFileController();

		if (!inpxFileExists)
		{
			zipFileController->AddFile("collection.info", QString("%1, favorites").arg(m_collectionProvider->GetActiveCollection().name).toUtf8(), QDateTime::currentDateTime());
			zipFileController->AddFile("version.info", QDateTime::currentDateTime().toString("yyyyMMdd").toUtf8(), QDateTime::currentDateTime());
		}

		for (auto&& [uid, data] : m_paths)
			if (!data.isEmpty())
				zipFileController->AddFile(QString("%1.inp").arg(uid), std::move(data), QDateTime::currentDateTime());

		Zip zip(inpxFileName, Zip::Format::Zip, inpxFileExists);
		zip.Write(std::move(zipFileController));

		m_callback(m_hasError);
	}

	QString GetInpxFileName() const
	{
		return QString("%1/collection.inpx").arg(m_dstFolder);
	}

	void GenerateInpxImpl(const QString& inpxFileName)
	{
		auto       db         = m_databaseUser->Database();
		const auto booksCount = [&] {
			auto query = db->CreateQuery("select count(42) from Books b left join Series_List sl on sl.BookID = b.BookID");
			query->Execute();
			assert(!query->Eof());
			return query->Get<long long>(0);
		}();

		auto bookProgressItem = m_progressController->Add(2 * booksCount);

		const auto dictionaryProgressSize = booksCount / 10;
		auto       dictionaryProgressItem = m_progressController->Add(dictionaryProgressSize);

		const auto genres     = GetGenres(*db);
		const auto bookGenres = GetBookGenres(*db, genres);
		dictionaryProgressItem->Increment(60 * dictionaryProgressSize / 100);
		const auto authors = GetAuthors(*db);
		dictionaryProgressItem->Increment(10 * dictionaryProgressSize / 100);
		const auto bookAuthors = GetBookAuthors(*db, authors);
		dictionaryProgressItem->Increment(20 * dictionaryProgressSize / 100);
		const auto series = GetSeries(*db);
		dictionaryProgressItem->Increment(5 * dictionaryProgressSize / 100);
		const auto keywords     = GetKeywords(*db);
		const auto bookKeywords = GetBookKeywords(*db, keywords);
		dictionaryProgressItem.reset();

		{
			if (QFile::exists(inpxFileName))
				QFile::remove(inpxFileName);

			auto zipFileController = Zip::CreateZipFileController();
			zipFileController->AddFile("collection.info", QString("%1").arg(m_collectionProvider->GetActiveCollection().name).toUtf8(), QDateTime::currentDateTime());
			zipFileController->AddFile("version.info", QDateTime::currentDateTime().toString("yyyyMMdd").toUtf8(), QDateTime::currentDateTime());

			Zip                                         zip(inpxFileName, Zip::Format::Zip);
			std::vector<std::pair<QString, QByteArray>> toZip;
			zip.Write(std::move(zipFileController));
		}

		QString    currentFolder;
		QByteArray data;
		int64_t    counter = 0;
		const auto write   = [&] {
            if (counter == 0)
                return;

            assert(!currentFolder.isEmpty() && !data.isEmpty());

            Zip zip(inpxFileName, Zip::Format::Auto, true);

            QFileInfo fileInfo(currentFolder);
            auto      zipFileController = Zip::CreateZipFileController();
            zipFileController->AddFile(fileInfo.completeBaseName() + ".inp", std::move(data), QDateTime::currentDateTime());
            zip.Write(std::move(zipFileController));

            bookProgressItem->Increment(counter);
            counter = 0;
		};

		const auto query = db->CreateQuery(BOOK_QUERY);
		for (query->Execute(); !query->Eof(); query->Next())
		{
			QString folder = query->Get<const char*>(8);
			if (currentFolder != folder)
			{
				write();
				currentFolder = std::move(folder);
				data          = {};
			}
			Write(data, *query, series, genres, bookGenres, authors, bookAuthors, keywords, bookKeywords);
			++counter;
			bookProgressItem->Increment(1);
		}
		write();
	}

private:
	std::weak_ptr<const ILogicFactory>                      m_logicFactory;
	std::shared_ptr<const ICollectionProvider>              m_collectionProvider;
	std::shared_ptr<const IDatabaseUser>                    m_databaseUser;
	PropagateConstPtr<IProgressController, std::shared_ptr> m_progressController;

	Callback                                m_callback;
	size_t                                  m_taskCount { 0 };
	bool                                    m_hasError { false };
	std::unique_ptr<Util::IExecutor>        m_executor;
	QString                                 m_dstFolder;
	std::filesystem::path                   m_archiveFolder;
	std::mutex                              m_pathsGuard;
	std::unordered_map<QString, QByteArray> m_paths;
};

InpxGenerator::InpxGenerator(
	const std::shared_ptr<const ILogicFactory>&        logicFactory,
	std::shared_ptr<const ICollectionProvider>         collectionProvider,
	std::shared_ptr<const IDatabaseUser>               databaseUser,
	std::shared_ptr<IBooksExtractorProgressController> progressController
)
	: m_impl(logicFactory, std::move(collectionProvider), std::move(databaseUser), std::move(progressController))
{
	PLOGV << "InpxGenerator created";
}

InpxGenerator::~InpxGenerator()
{
	PLOGV << "InpxGenerator destroyed";
}

void InpxGenerator::ExtractAsInpxCollection(QString folder, const std::vector<QString>& idList, const IBookInfoProvider& dataProvider, Callback callback)
{
	PLOGD << QString("Extract %1 books as inpx-collection started").arg(idList.size());

	std::vector<BookInfo> bookInfo;
	std::ranges::transform(idList, std::back_inserter(bookInfo), [&](const auto& id) {
		return dataProvider.GetBookInfo(id.toLongLong());
	});

	m_impl->Extract(std::move(folder), std::move(bookInfo), std::move(callback));
}

void InpxGenerator::GenerateInpx(QString inpxFileName, const std::vector<QString>& idList, const IBookInfoProvider& dataProvider, Callback callback)
{
	std::vector<BookInfo> bookInfo;
	std::ranges::transform(idList, std::back_inserter(bookInfo), [&](const auto& id) {
		return dataProvider.GetBookInfo(id.toLongLong());
	});

	m_impl->GenerateInpx(std::move(inpxFileName), std::move(bookInfo), std::move(callback));
}

void InpxGenerator::GenerateInpx(QString inpxFileName, Callback callback)
{
	m_impl->GenerateInpx(std::move(inpxFileName), std::move(callback));
}
