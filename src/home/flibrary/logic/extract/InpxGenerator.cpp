#include "InpxGenerator.h"

#include <filesystem>
#include <mutex>
#include <ranges>

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QUuid>

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/logic/IDataProvider.h"

#include "data/DataItem.h"
#include "platform/StrUtil.h"
#include "util/Fb2InpxParser.h"
#include "util/FunctorExecutionForwarder.h"
#include "util/IExecutor.h"
#include "util/ImageRestore.h"

#include "Constant.h"
#include "log.h"
#include "zip.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

using Genres = std::unordered_map<QString, QString>;

Genres GetGenres(DB::IDatabase& db)
{
	Genres result;

	const auto query = db.CreateQuery("select GenreCode, FB2Code, ParentCode from Genres");
	for (query->Execute(); !query->Eof(); query->Next())
		result.try_emplace(query->Get<const char*>(0), query->Get<const char*>(1));

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

//"AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;INSNO;FOLDER;LANG;LIBRATE;KEYWORDS;"
QByteArray Write(const BookInfo& book, const QString& fileName, const size_t n, const QString& folder, const QString& size)
{
	QByteArray stream;

	auto seqNumber = book.book->GetRawData(BookItem::Column::SeqNumber);
	if (seqNumber.toInt() <= 0)
		seqNumber.clear();

	const QFileInfo fileInfo(fileName);

	QStringList authors;
	for (const auto& author : book.authors)
	{
		const QStringList authorNames = QStringList() << author->GetRawData(AuthorItem::Column::LastName) << author->GetRawData(AuthorItem::Column::FirstName)
		                                              << author->GetRawData(AuthorItem::Column::MiddleName);
		authors << authorNames.join(Util::Fb2InpxParser::NAMES_SEPARATOR) << QString(Inpx::LIST_SEPARATOR);
	}
	stream.append((authors.join("") + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());

	for (const auto& genre : book.genres)
		stream.append((genre->GetRawData(GenreItem::Column::Fb2Code) + Inpx::LIST_SEPARATOR).toUtf8());
	stream.append(QString(Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());

	stream.append((book.book->GetRawData(BookItem::Column::Title) + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((book.book->GetRawData(BookItem::Column::Series) + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((seqNumber + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((fileInfo.completeBaseName() + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((size + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((book.book->GetId() + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((QString(book.book->IsRemoved() ? "1" : "0") + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((fileInfo.suffix() + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((book.book->GetRawData(BookItem::Column::UpdateDate) + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((QString::number(n) + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((folder + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((book.book->GetRawData(BookItem::Column::Lang) + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((book.book->GetRawData(BookItem::Column::LibRate) + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());
	stream.append((QString() + Util::Fb2InpxParser::FIELDS_SEPARATOR).toUtf8());

	stream.append("\n");

	return stream;
}

constexpr auto BOOK_QUERY = R"(
select
b.BookID, b.LibID, b.Title, sl.SeriesID, sl.SeqNumber, b.UpdateDate, b.LibRate, b.Lang, f.FolderTitle, b.FileName, b.Ext, b.BookSize, coalesce(bu.IsDeleted, b.IsDeleted)
from Books b
join Folders f on f.FolderID = b.FolderID
left join Books_User bu on bu.BookID = b.BookID
left join Series_List sl on sl.BookID = b.BookID
order by f.FolderID
)";

QString Write(
	const DB::IQuery&   query,
	const Series&       series,
	const Genres&       genres,
	const BookGenres&   bookGenres,
	const Authors&      authors,
	const BookAuthors&  bookAuthors,
	const Keywords&     keywords,
	const BookKeywords& bookKeywords,
	const size_t        n
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
				return f(itemIt->second) + Inpx::LIST_SEPARATOR;
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

	//	"AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;INSNO;FOLDER;LANG;LIBRATE;KEYWORDS;"
	auto book = QStringList {} << authorList.join("") << genreList.join("") << query.Get<const char*>(2) << seriesTitle << Util::Fb2InpxParser::GetSeqNumber(query.Get<const char*>(4))
	                           << query.Get<const char*>(9) << QString::number(query.Get<long long>(11)) << query.Get<const char*>(1) << QString::number(query.Get<int>(12)) << query.Get<const char*>(10) + 1
	                           << query.Get<const char*>(5) << QString::number(n) << query.Get<const char*>(8) << query.Get<const char*>(7) << Util::Fb2InpxParser::GetSeqNumber(query.Get<const char*>(6))
	                           << keywordList.join("");

	return book.join(Util::Fb2InpxParser::FIELDS_SEPARATOR).append(Util::Fb2InpxParser::FIELDS_SEPARATOR).append("\r\n");
}

class IProgress // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IProgress()            = default;
	virtual void Increment(int64_t) = 0;
	virtual bool IsStopped() const  = 0;
};

void Unpack(
	const std::filesystem::path&                archiveFolder,
	const QString&                              dstFolder,
	const BookInfoList&                         bookInfoLists,
	const std::unordered_map<QString, QString>& idToFileName,
	IProgress&                                  progress,
	const ISettings&                            settings
)
{
	if (bookInfoLists.empty())
		return;

	const auto archivePath = Platform::PathToString(archiveFolder / Platform::StringToPath(bookInfoLists.front().book->GetRawData(BookItem::Column::Folder)));
	const Zip  zip(archivePath);

	for (const auto& book : bookInfoLists)
	{
		if (progress.IsStopped())
			break;

		auto       fileName = book.book->GetRawData(BookItem::Column::FileName);
		const auto input    = zip.Read(fileName);
		const auto bytes    = Util::PrepareToExport(input->GetStream(), archivePath, fileName, settings);
		if (const auto it = idToFileName.find(book.book->GetId()); it != idToFileName.end())
			fileName = it->second;

		const auto outputPath = dstFolder + "/" + fileName;
		QFile      file(outputPath);
		if (!file.open(QIODevice::WriteOnly)) [[unlikely]]
		{
			PLOGE << "Cannot write to " + outputPath;
			break;
		}

		file.write(bytes);
		progress.Increment(1);
	}
}

class ZipProgressCallback final : public Zip::ProgressCallback
{
public:
	explicit ZipProgressCallback(IProgress& progress)
		: m_progress { progress }
	{
	}

private: // ProgressCallback
	void OnStartWithTotal(int64_t) override
	{
	}

	void OnIncrement(int64_t) override
	{
	}

	void OnSetCompleted(int64_t) override
	{
	}

	void OnDone() override
	{
	}

	void OnFileDone(const QString&) override
	{
		m_progress.Increment(1);
	}

	bool OnCheckBreak() override
	{
		return m_progress.IsStopped();
	}

private:
	IProgress& m_progress;
};

QByteArray Pack(const QString& dstFolder, const QString& uid, const QString& booksFolder, const BookInfoList& bookInfoLists, const std::unordered_map<QString, QString>& idToFileName, IProgress& progress)
{
	assert(!bookInfoLists.empty());

	QByteArray inpx;

	const auto zipFiles   = Zip::CreateZipFileController();
	const auto fileFolder = uid + ".zip";

	for (auto&& [book, n] : std::views::zip(bookInfoLists, std::views::iota(1)))
	{
		if (progress.IsStopped())
			break;

		auto fileName = book.book->GetRawData(BookItem::Column::FileName);
		if (const auto it = idToFileName.find(book.book->GetId()); it != idToFileName.end())
			fileName = it->second;

		const auto inputPath = booksFolder + "/" + fileName;
		{
			QFile file(inputPath);
			if (!file.open(QIODevice::ReadOnly))
			{
				PLOGE << "Cannot read from " + inputPath;
				break;
			}

			const auto bytes = file.readAll();
			zipFiles->AddFile(fileName, bytes, QDateTime::currentDateTime());
			inpx.append(Write(book, fileName, n, fileFolder, QString::number(bytes.size())));
		}
		QFile::remove(inputPath);
	}

	Zip zip(QString("%1/%2.zip").arg(dstFolder, uid), Zip::Format::Zip, false, std::make_shared<ZipProgressCallback>(progress));
	zip.Write(*zipFiles);
	return inpx;
}

} // namespace

class InpxGenerator::Impl final : public IProgress
{
	NON_COPY_MOVABLE(Impl)

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
		, m_archiveFolder { Platform::StringToPath(m_collectionProvider->GetActiveCollection().GetFolder()) }
	{
	}

	~Impl() override
	{
		if (!m_tmpFolder.isEmpty())
			if (QDir dir(m_tmpFolder); dir.exists())
				dir.removeRecursively();
	}

	void Extract(QString inpxFileName, BookInfoList&& books, Callback callback)
	{
		const auto logicFactory = ILogicFactory::Lock(m_logicFactory);

		assert(!m_callback);
		m_hasError     = false;
		m_callback     = std::move(callback);
		m_settingsStub = logicFactory->CreateSettingsStub();
		m_inpxFileName = std::move(inpxFileName);
		m_dstFolder    = QFileInfo(m_inpxFileName).absoluteDir().absolutePath();
		m_tmpFolder    = m_dstFolder + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces);
		m_progressItem = m_progressController->Add(std::ssize(books) * 2 + 1);

		QDir(m_tmpFolder).mkpath(".");
		const auto outputFolderCount = std::size(books) / 3200 + 1;

		m_chunkSize = std::size(books) / outputFolderCount;
		if (outputFolderCount * m_chunkSize < std::size(books))
			++m_chunkSize;

		std::unordered_map<QString, std::pair<QString, std::vector<QString>>> uniqueFileNames;

		std::unordered_map<QString, BookInfoList> ordered;
		for (auto&& book : books)
		{
			auto fileName    = book.book->GetRawData(BookItem::Column::FileName); //-V821
			auto& [key, ids] = uniqueFileNames[fileName.toLower()];
			if (key.isEmpty())
				key = std::move(fileName);
			ids.emplace_back(book.book->GetId());
			const auto folder = book.book->GetRawData(BookItem::Column::Folder);
			ordered[folder].emplace_back(std::move(book));
		}

		for (auto&& [fileName, ids] : uniqueFileNames | std::views::values | std::views::filter([](const auto& item) {
										  return item.second.size() > 1;
									  }))
		{
			for (auto&& [id, index] : std::views::zip(ids | std::views::drop(1), std::views::iota(1)))
			{
				const QFileInfo fileInfo(fileName);
				m_idToFileName.try_emplace(std::move(id), QString("%1_%2.%3").arg(fileInfo.completeBaseName()).arg(index).arg(fileInfo.suffix()));
			}
		}

		logicFactory->GetExecutor({ static_cast<int>(outputFolderCount) + static_cast<int>(ordered.size()) }).swap(m_executor);

		for (auto&& item : ordered | std::views::values)
			(*m_executor)(CreateUnpackTask(std::move(item)));
	}

	void GenerateInpx(QString inpxFileName, BookInfoList&& books, Callback callback)
	{
		assert(!m_callback);
		m_hasError = false;
		m_callback = std::move(callback);
		ILogicFactory::Lock(m_logicFactory)->GetExecutor().swap(m_executor);
		m_progressItem = m_progressController->Add(static_cast<int64_t>(books.size()) + 1);

		(*m_executor)({ "Create inpx", [this, books = std::move(books), inpxFileName = std::move(inpxFileName)]() mutable {
						   std::ranges::sort(books, {}, [](const auto& item) {
							   return item.book->GetRawData(BookItem::Column::Folder);
						   });
						   QString                      currentFolder;
						   std::unique_ptr<Zip>         currentZip;
						   std::map<size_t, QByteArray> data;

						   const auto write = [&] {
							   if (data.empty())
								   return;

							   QByteArray stream;
							   for (auto&& str : data | std::views::values)
								   stream.append(str);

							   m_paths.try_emplace(QFileInfo(currentFolder).completeBaseName(), std::move(stream));
							   data.clear();
						   };

						   for (auto&& book : books)
						   {
							   if (m_progressItem->IsStopped())
								   break;

							   const auto folder = book.book->GetRawData(BookItem::Column::Folder);
							   if (currentFolder != folder)
							   {
								   write();
								   currentFolder = folder;
								   currentZip    = std::make_unique<Zip>(Platform::PathToString(m_archiveFolder / Platform::StringToPath(currentFolder)));
							   }

							   const auto fileName = book.book->GetRawData(BookItem::Column::FileName);
							   assert(currentZip);
							   if (const auto n = currentZip->GetFileIndex(fileName); n != Zip::INVALID_INDEX) //-V614
								   data.try_emplace(n, Write(book, fileName, n + 1, book.book->GetRawData(BookItem::Column::Folder), book.book->GetRawData(BookItem::Column::Size)));

							   m_progressItem->Increment(1);
						   }
						   write();
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

private: // IProgress
	void Increment(const int64_t value) override
	{
		m_forwarder.Forward([this, value] {
			m_progressItem->Increment(value);
		});
	}

	bool IsStopped() const override
	{
		return m_progressItem->IsStopped();
	}

private:
	void OnStopped() const
	{
		if (m_packTaskCount + m_unpackTaskCount == 0)
			m_callback(false);
	}

	Util::IExecutor::Task CreateUnpackTask(BookInfoList&& books)
	{
		++m_unpackTaskCount;
		auto taskName = books.front().book->GetRawData(BookItem::Column::Folder).toStdString();
		return Util::IExecutor::Task { std::move(taskName), [this, books = std::move(books)]() mutable {
										  bool       error = false;
										  QByteArray inpx;
										  try
										  {
											  Unpack(m_archiveFolder, m_tmpFolder, books, m_idToFileName, *this, *m_settingsStub);
										  }
										  catch (const std::exception& ex)
										  {
											  PLOGE << ex.what();
											  error = true;
										  }
										  return [this, error, inpx = std::move(inpx), books = std::move(books)](const size_t) mutable {
											  --m_unpackTaskCount;
											  if (m_progressItem->IsStopped())
												  return OnStopped();

											  std::ranges::move(books, std::back_inserter(m_unpackedBooks));

											  while (m_unpackedBooks.size() >= m_chunkSize)
												  (*m_executor)(CreatePackTask(), 1, false);

											  if (m_unpackTaskCount == 0 && !m_unpackedBooks.empty())
												  (*m_executor)(CreatePackTask(), 1, false);
										  };
									  } };
	}

	Util::IExecutor::Task CreatePackTask()
	{
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

		BookInfoList books;
		std::ranges::move(m_unpackedBooks | std::views::drop(m_unpackedBooks.size() > m_chunkSize ? m_unpackedBooks.size() - m_chunkSize : 0), std::back_inserter(books));
		m_unpackedBooks.resize(m_unpackedBooks.size() - books.size());

		auto taskName = uid.toStdString();
		++m_packTaskCount;
		return Util::IExecutor::Task { std::move(taskName), [this, books = std::move(books), uid = std::move(uid)]() mutable {
										  bool       error = false;
										  QByteArray inpx;
										  try
										  {
											  inpx = Pack(m_dstFolder, uid, m_tmpFolder, books, m_idToFileName, *this);
										  }
										  catch (const std::exception& ex)
										  {
											  PLOGE << ex.what();
											  error = true;
										  }
										  return [this, error, inpx = std::move(inpx), uid = std::move(uid), unpackedCount = std::size(books)](const size_t) mutable {
											  --m_packTaskCount;
											  if (m_progressItem->IsStopped())
												  return OnStopped();

											  {
												  std::lock_guard lock(m_pathsGuard);
												  m_paths[uid] = std::move(inpx);
											  }

											  m_hasError = error || m_hasError;
											  assert(m_packTaskCount > 0);
											  if (m_packTaskCount > 0 || m_unpackTaskCount > 0)
												  return;

											  Increment(1);
											  assert(m_unpackedBooks.empty());
											  WriteInpx(m_inpxFileName);
										  };
									  } };
	}

	void WriteInpx(const QString& inpxFileName)
	{
		const auto inpxFileExists = QFile::exists(inpxFileName);

		auto zipFileController = Zip::CreateZipFileController();

		if (!inpxFileExists)
		{
			zipFileController->AddFile(Inpx::COLLECTION_INFO, QString("%1, favorites").arg(m_collectionProvider->GetActiveCollection().name).toUtf8(), QDateTime::currentDateTime());
			zipFileController->AddFile(Inpx::VERSION_INFO, QDateTime::currentDateTime().toString("yyyyMMdd").toUtf8(), QDateTime::currentDateTime());
			zipFileController->AddFile(Inpx::STRUCTURE_INFO, "AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;INSNO;FOLDER;LANG;LIBRATE;KEYWORDS;", QDateTime::currentDateTime());
		}

		for (const auto& [uid, data] : m_paths)
			if (!data.isEmpty())
				zipFileController->AddFile(QString("%1.inp").arg(uid), data, QDateTime::currentDateTime());

		Zip zip(inpxFileName, Zip::Format::Zip, inpxFileExists);
		zip.Write(*zipFileController);

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

		m_progressItem = m_progressController->Add(booksCount + 4);

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

		if (QFile::exists(inpxFileName))
			QFile::remove(inpxFileName);

		auto zipFileController = Zip::CreateZipFileController();
		zipFileController->AddFile(Inpx::COLLECTION_INFO, QString("%1").arg(m_collectionProvider->GetActiveCollection().name).toUtf8(), QDateTime::currentDateTime());
		zipFileController->AddFile(Inpx::VERSION_INFO, QDateTime::currentDateTime().toString("yyyyMMdd").toUtf8(), QDateTime::currentDateTime());
		zipFileController->AddFile(Inpx::STRUCTURE_INFO, "AUTHOR;GENRE;TITLE;SERIES;SERNO;FILE;SIZE;LIBID;DEL;EXT;DATE;INSNO;FOLDER;LANG;LIBRATE;KEYWORDS;", QDateTime::currentDateTime());

		std::unique_ptr<Zip> inputZip;

		QString                   currentFolder;
		std::map<size_t, QString> data;
		const auto                write = [&] {
			if (data.empty())
				return;

			assert(!currentFolder.isEmpty());
			QFileInfo  fileInfo(currentFolder);
			QByteArray bytes;
			for (const auto& str : data | std::views::values)
				bytes.append(str.toUtf8());

			zipFileController->AddFile(fileInfo.completeBaseName() + ".inp", bytes, QDateTime::currentDateTime());
		};

		const auto query = db->CreateQuery(BOOK_QUERY);
		for (query->Execute(); !query->Eof(); query->Next())
		{
			if (m_progressItem->IsStopped())
				return;

			QString folder = query->Get<const char*>(8);
			if (currentFolder != folder)
			{
				write();
				data          = {};
				currentFolder = std::move(folder);
				inputZip      = std::make_unique<Zip>(Platform::PathToString(m_archiveFolder / Platform::StringToPath(currentFolder)));
				m_progressItem->Increment(-1);
			}
			assert(inputZip);
			if (const auto index = inputZip->GetFileIndex(QString("%1%2").arg(query->Get<const char*>(9), query->Get<const char*>(10))); index != Zip::INVALID_INDEX) //-V614
				data.try_emplace(index, Write(*query, series, genres, bookGenres, authors, bookAuthors, keywords, bookKeywords, index + 1));

			m_progressItem->Increment(1);
		}
		write();

		Zip zip(inpxFileName, Zip::Format::Zip, false, std::make_shared<ZipProgressCallback>(*this));
		zip.Write(*zipFileController);
	}

private:
	std::weak_ptr<const ILogicFactory>                      m_logicFactory;
	std::shared_ptr<const ICollectionProvider>              m_collectionProvider;
	std::shared_ptr<const IDatabaseUser>                    m_databaseUser;
	std::shared_ptr<const ISettings>                        m_settingsStub;
	PropagateConstPtr<IProgressController, std::shared_ptr> m_progressController;

	Callback m_callback;

	BookInfoList m_unpackedBooks;

	std::unique_ptr<IProgressController::IProgressItem> m_progressItem;
	Util::FunctorExecutionForwarder                     m_forwarder;

	std::unordered_map<QString, QString>    m_idToFileName;
	size_t                                  m_unpackTaskCount { 0 };
	size_t                                  m_packTaskCount { 0 };
	bool                                    m_hasError { false };
	size_t                                  m_chunkSize { 0 };
	std::unique_ptr<Util::IExecutor>        m_executor;
	QString                                 m_inpxFileName;
	QString                                 m_dstFolder;
	std::filesystem::path                   m_archiveFolder;
	QString                                 m_tmpFolder;
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

void InpxGenerator::ExtractAsInpxCollection(QString inpxFileName, const std::vector<QString>& idList, const IBookInfoProvider& dataProvider, Callback callback)
{
	PLOGD << QString("Extract %1 books as inpx-collection started").arg(idList.size());

	std::vector<BookInfo> bookInfo;
	std::ranges::transform(idList, std::back_inserter(bookInfo), [&](const auto& id) {
		return dataProvider.GetBookInfo(id.toLongLong());
	});

	m_impl->Extract(std::move(inpxFileName), std::move(bookInfo), std::move(callback));
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
