#include "CollectionCleaner.h"

#include <ranges>

#include <QFileInfo>

#include "fnd/algorithm.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "common/Constant.h"
#include "database/DatabaseUtil.h"
#include "util/Fb2InpxParser.h"

#include "Zip.h"
#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

struct AnalyzedBook
{
	QString             folder;
	QString             file;
	QString             lang;
	bool                deleted;
	QString             date;
	QString             title;
	size_t              size;
	QString             libId;
	double              libRate;
	std::set<QString>   genres;
	std::set<long long> authors;
};

using AnalyzedBooks = std::unordered_map<long long, AnalyzedBook>;

constexpr auto SELECT_ANALYZED_BOOKS_QUERY = R"(
select b.BookID, f.FolderTitle, b.FileName || b.Ext, b.Lang, coalesce(bu.IsDeleted, b.IsDeleted, 0), b.UpdateDate, b.Title, b.BookSize, b.LibID, b.LibRate, %1, %3 
	from Books b 
	join Folders f on f.FolderID = b.FolderID %2 %4 
	left join Books_User bu on bu.BookID = b.BookID 
	%5
)";

constexpr auto EMPTY_FIELD       = "42";
constexpr auto GENRE_JOIN        = "\n	join Genre_List gl on gl.BookID = b.BookID";
constexpr auto GENRE_FIELD       = "gl.GenreCode";
constexpr auto AUTHOR_JOIN       = "\n	join Author_List al on al.BookID = b.BookID";
constexpr auto AUTHOR_FIELD      = "al.AuthorID";
constexpr auto WHERE_NOT_DELETED = "where coalesce(bu.IsDeleted, b.IsDeleted, 0) = 0";

bool RemoveBooksImpl(const ICollectionCleaner::Books& books, DB::ITransaction& transaction, std::unique_ptr<IProgressController::IProgressItem> progressItem)
{
	PLOGI << "Removing database books records started";

	return std::accumulate(
		books.cbegin(),
		books.cend(),
		true,
		[&,
	     progressItem = std::move(progressItem),
	     command      = transaction.CreateCommand("delete from Books where BookID = ?"),
	     total        = books.size(),
	     currentPct   = 0,
	     lastPct      = 0,
	     lastPctLog   = 0,
	     n            = size_t { 0 }](const bool init, const auto& book) mutable {
			++n;
			currentPct = static_cast<int>(100U * n / total);
			progressItem->Increment(currentPct - lastPct);
			lastPct = currentPct;

			if (!(lastPct % 10) && lastPctLog != lastPct)
			{
				lastPctLog = lastPct;
				PLOGV << lastPct << '%';
			}

			command->Bind(0, book.id);
			return command->Execute() && init;
		}
	);
}

bool CleanupNavigationItems(DB::ITransaction& transaction)
{
	PLOGI << "Removing database book references records started";
	constexpr std::pair<const char*, const char*> commands[] {
		{         "Series_List",               "delete from Series_List where not exists (select 42 from Books where Books.BookID = Series_List.BookID)" },
		{			  "Series",         "delete from Series where not exists (select 42 from Series_List where Series_List.SeriesID = Series.SeriesID)" },
		{		  "Genre_List",                 "delete from Genre_List where not exists (select 42 from Books where Books.BookID = Genre_List.BookID)" },
		{         "Author_List",               "delete from Author_List where not exists (select 42 from Books where Books.BookID = Author_List.BookID)" },
		{			 "Authors",       "delete from Authors where not exists (select 42 from Author_List where Author_List.AuthorID = Authors.AuthorID)" },
		{        "Keyword_List",             "delete from Keyword_List where not exists (select 42 from Books where Books.BookID = Keyword_List.BookID)" },
		{			"Keywords", "delete from Keywords where not exists (select 42 from Keyword_List where Keyword_List.KeywordID = Keywords.KeywordID)" },
		{			 "Reviews",					   "delete from Reviews where not exists (select 42 from Books where Books.BookID = Reviews.BookID)" },
		{    "Groups_List_User",																				  R"(delete from Groups_List_User where
not exists (select 42 from Books b where b.BookID = Groups_List_User.ObjectID) and 
not exists (select 42 from Authors a where a.AuthorID = Groups_List_User.ObjectID) and 
not exists (select 42 from Series s where s.SeriesID = Groups_List_User.ObjectID) and 
not exists (select 42 from Keywords k where k.KeywordID = Groups_List_User.ObjectID))" },
		{       "Update months", "delete from Updates where ParentID != 0 and not exists (select 42 from Books where Updates.UpdateID = Books.UpdateID)" },
		{        "Update years",  "delete from Updates where ParentID = 0 and not exists (select 42 from Updates u where u.ParentID = Updates.UpdateID)" },
		{        "Books_Search",															  "insert into Books_Search(Books_Search) values('rebuild')" },
		{      "Authors_Search",														  "insert into Authors_Search(Authors_Search) values('rebuild')" },
		{       "Series_Search",															"insert into Series_Search(Series_Search) values('rebuild')" },
		{ "Compilations_Search",												"insert into Compilations_Search(Compilations_Search) values('rebuild')" },
	};
	return std::accumulate(std::cbegin(commands), std::cend(commands), true, [&](const bool init, const auto& command) {
		PLOGD << "removing from " << command.first;
		return transaction.CreateCommand(command.second)->Execute() && init;
	});
}

using AllFilesItem = std::tuple<std::vector<QString>, std::shared_ptr<Zip::ProgressCallback>>;
using AllFiles     = std::unordered_map<QString, AllFilesItem>;

AllFiles CollectBookFiles(ICollectionCleaner::Books& books, const ILogicFactory& logicFactory, const std::shared_ptr<IProgressController>& progressController)
{
	AllFiles result;
	for (auto&& [id, folder, file] : books)
	{
		auto [it, ok]           = result.try_emplace(std::move(folder), AllFilesItem {});
		auto& [files, progress] = it->second;
		files.emplace_back(std::move(file));
		if (ok)
			progress = logicFactory.CreateZipProgressCallback(progressController);
	}
	return result;
}

auto GetFolderPath(const QString& collectionFolder, const QString& name)
{
	return QString("%1/%2").arg(collectionFolder, name);
}

AllFiles CollectImageFiles(const AllFiles& bookFiles, const QString& collectionFolder, const ILogicFactory& logicFactory, const std::shared_ptr<IProgressController>& progressController)
{
	static constexpr std::pair<const char*, const char*> imageTypes[] {
		{ Global::COVERS,   "" },
		{ Global::IMAGES, "/*" },
	};

	AllFiles result;
	for (const auto& [folder, archiveItem] : bookFiles)
	{
		const QFileInfo fileInfo(folder);
		for (const auto& [type, replacedExt] : imageTypes)
		{
			const auto imageFolderName = QString("%1/%2.zip").arg(type, fileInfo.completeBaseName());
			if (QFile::exists(GetFolderPath(collectionFolder, imageFolderName)))
			{
				auto& [files, progress] = result[imageFolderName];
				std::ranges::transform(std::get<0>(archiveItem), std::back_inserter(files), [=](const QString& file) {
					return QFileInfo(file).completeBaseName() + replacedExt;
				});
				progress = logicFactory.CreateZipProgressCallback(progressController);
			}
		}
	}

	return result;
}

void RemoveFiles(AllFiles& allFiles, const QString& collectionFolder)
{
	std::map toCleanup(std::make_move_iterator(allFiles.begin()), std::make_move_iterator(allFiles.end()));
	for (auto&& [folder, archiveItem] : toCleanup)
	{
		const auto archive = GetFolderPath(collectionFolder, folder);
		if ([&] {
				auto&& [files, progressCallback] = archiveItem;
				Zip zip(archive, Zip::Format::Auto, true, std::move(progressCallback));
				zip.Remove(files);
				return zip.GetFileNameList().isEmpty();
			}())
		{
			QFile::remove(archive);
		}
	}
}

AnalyzedBooks GetAnalysedBooks(DB::IDatabase& db, const ILibRateProvider& libRateProvider, const ICollectionCleaner::IAnalyzeObserver& observer, const bool hasGenres, const std::atomic_bool& analyzeCanceled)
{
	const auto query = db.CreateQuery(QString(SELECT_ANALYZED_BOOKS_QUERY)
	                                      .arg(hasGenres ? GENRE_FIELD : EMPTY_FIELD)
	                                      .arg(hasGenres ? GENRE_JOIN : "")
	                                      .arg(observer.NeedDeleteDuplicates() ? AUTHOR_FIELD : EMPTY_FIELD)
	                                      .arg(observer.NeedDeleteDuplicates() ? AUTHOR_JOIN : "")
	                                      .arg(observer.IsPermanently() ? "" : WHERE_NOT_DELETED)
	                                      .toStdString());

	AnalyzedBooks analyzedBooks;
	for (query->Execute(); !query->Eof(); query->Next())
	{
		auto [it, inserted] = analyzedBooks.try_emplace(query->Get<long long>(0), AnalyzedBook {});
		if (inserted)
		{
			it->second.folder  = query->Get<const char*>(1);
			it->second.file    = query->Get<const char*>(2);
			it->second.lang    = query->Get<const char*>(3);
			it->second.deleted = query->Get<int>(4);
			it->second.date    = query->Get<const char*>(5);
			it->second.title   = QString(query->Get<const char*>(6)).toLower();
			it->second.size    = query->Get<long long>(7);
			it->second.libId   = query->Get<const char*>(8);
			it->second.libRate = libRateProvider.GetLibRate(it->second.libId, Util::Fb2InpxParser::GetSeqNumber(query->Get<const char*>(9)));
		}

		it->second.genres.emplace(query->Get<const char*>(10));
		it->second.authors.emplace(query->Get<long long>(11));
		if (analyzeCanceled)
			break;
	}

	return analyzedBooks;
}

void RemoveDuplicates(const AnalyzedBooks& analysedBooks, std::unordered_set<long long>& toDelete)
{
	std::unordered_map<QString, std::multimap<QString, long long, std::greater<>>> duplicates;
	for (const auto& [id, book] : analysedBooks)
		if (!toDelete.contains(id))
			duplicates[book.title].emplace(QString(book.date).append("/").append(book.file), id);

	for (auto& dup : duplicates | std::views::values | std::views::filter([](const auto& item) {
						 return item.size() > 1;
					 }))
	{
		while (!dup.empty())
		{
			const auto id = dup.begin()->second;
			dup.erase(dup.begin());
			const auto itOrigin = analysedBooks.find(id);
			assert(itOrigin != analysedBooks.end());

			for (auto iit = dup.begin(); iit != dup.end();)
			{
				const auto itCopy = analysedBooks.find(iit->second);
				assert(itCopy != analysedBooks.end());
				if (!Util::Intersect(itCopy->second.authors, itOrigin->second.authors))
				{
					++iit;
					continue;
				}

				toDelete.emplace(iit->second);
				iit = dup.erase(iit);
			}
		}
	}
}

} // namespace

struct CollectionCleaner::Impl
{
	std::weak_ptr<const ILogicFactory>                 logicFactory;
	std::shared_ptr<const IDatabaseUser>               databaseUser;
	std::shared_ptr<const ICollectionProvider>         collectionProvider;
	std::shared_ptr<const ILibRateProvider>            libRateProvider;
	std::shared_ptr<IBooksExtractorProgressController> progressController;
	std::atomic_bool                                   analyzeCanceled { false };

	void Analyze(IAnalyzeObserver& observer) const
	{
		databaseUser->Execute({ "Analyze", [&] {
								   std::function result { [&](size_t) {
									   observer.AnalyzeFinished({});
								   } };
								   auto genres    = observer.GetGenres();
								   auto languages = observer.GetLanguages();
								   if (genres.isEmpty() && languages.isEmpty() && !observer.NeedDeleteDuplicates() && !observer.NeedDeleteMarkedAsDeleted() && !observer.GetMinimumBookSize()
			                           && !observer.GetMaximumBookSize() && !observer.NeedDeleteUnrated() && !observer.GetMinimumLibRate())
									   return result;

								   const auto db = databaseUser->Database();
								   PLOGI << "get books info";
								   auto analysedBooks = GetAnalysedBooks(*db, *libRateProvider, observer, !genres.isEmpty(), analyzeCanceled);
								   PLOGI << "total books found: " << analysedBooks.size();

								   std::unordered_set<long long> toDelete;

								   const auto addToDelete = [&](const QString& name, const auto filter) {
									   const auto n = toDelete.size();
									   std::ranges::transform(analysedBooks | std::views::filter(filter), std::inserter(toDelete, toDelete.end()), [](const auto& item) {
										   return item.first;
									   });
									   PLOGI << name << " found: " << toDelete.size() - n;
								   };

								   if (observer.NeedDeleteMarkedAsDeleted())
									   addToDelete("marked as deleted", [](const auto& item) {
										   return item.second.deleted;
									   });

								   if (observer.NeedDeleteUnrated())
									   addToDelete("unrated", [](const auto& item) {
										   return item.second.libRate < 1.0;
									   });

								   if (auto value = observer.GetMinimumLibRate())
									   addToDelete("rated less then minimum", [value = *value - std::numeric_limits<double>::epsilon()](const auto& item) {
										   return item.second.libRate > std::numeric_limits<double>::epsilon() && item.second.libRate < value;
									   });

								   if (auto value = observer.GetMinimumBookSize())
									   addToDelete("smaller then minimum", [value = *value](const auto& item) {
										   return item.second.size < value;
									   });

								   if (auto value = observer.GetMaximumBookSize())
									   addToDelete("larger then maximum", [value = *value](const auto& item) {
										   return item.second.size > value;
									   });

								   if (!languages.isEmpty())
									   addToDelete(
										   "on specified languages",
										   [languages = std::unordered_set(std::make_move_iterator(languages.begin()), std::make_move_iterator(languages.end()))](const auto& item) {
											   return languages.contains(item.second.lang);
										   }
									   );
								   languages.clear();

								   if (!genres.isEmpty())
								   {
									   const auto indexedGenres = std::set(std::make_move_iterator(genres.begin()), std::make_move_iterator(genres.end()));
									   switch (observer.GetCleanGenreMode()) // NOLINT(clang-diagnostic-switch-enum)
									   {
										   case CleanGenreMode::Full:
											   addToDelete("in specified genres full", [&](const auto& item) {
												   return std::ranges::includes(indexedGenres, item.second.genres);
											   });
											   break;
										   case CleanGenreMode::Partial:
											   addToDelete("in specified genres partial", [&](const auto& item) {
												   return Util::Intersect(indexedGenres, item.second.genres);
											   });
											   break;
										   default: // NOLINT(clang-diagnostic-covered-switch-default)
											   assert(false && "unexpected mode");
											   break;
									   }
									   genres.clear();
								   }

								   if (observer.NeedDeleteDuplicates())
								   {
									   const auto n = toDelete.size();
									   RemoveDuplicates(analysedBooks, toDelete);
									   PLOGI << "duplicates found: " << toDelete.size() - n;
								   }

								   Books books;
								   books.reserve(toDelete.size());
								   std::ranges::transform(toDelete, std::back_inserter(books), [&](const long long id) {
									   const auto it = analysedBooks.find(id);
									   assert(it != analysedBooks.end());
									   return Book { id, std::move(it->second.folder), std::move(it->second.file) };
								   });

								   analysedBooks.clear();
								   toDelete.clear();

								   result = [&, books = std::move(books)](size_t) mutable {
									   observer.AnalyzeFinished(std::move(books));
								   };

								   return result;
							   } });
	}

	void AnalyzeCancel()
	{
		analyzeCanceled = true;
	}

	void Remove(Books books, Callback callback) const
	{
		databaseUser->Execute({ "Delete books permanently", [this, books = std::move(books), callback = std::move(callback)]() mutable {
								   std::unordered_set<long long> ids;
								   std::ranges::transform(books, std::inserter(ids, ids.end()), [](const auto& book) {
									   return book.id;
								   });
								   const bool ok = DatabaseUtil::ChangeBookRemoved(*databaseUser->Database(), ids, true, progressController);
								   return [callback = std::move(callback), ok](size_t) {
									   callback(ok);
								   };
							   } });
	}

	void RemovePermanently(Books books, Callback callback) const
	{
		databaseUser->Execute({ "Delete books permanently", [this, books = std::move(books), callback = std::move(callback), collectionFolder = collectionProvider->GetActiveCollection().folder]() mutable {
								   auto progressItem = progressController->Add(100);

								   auto logicFactoryPtr = ILogicFactory::Lock(logicFactory);
								   auto allFiles        = CollectBookFiles(books, *logicFactoryPtr, progressController);
								   auto images          = CollectImageFiles(allFiles, collectionFolder, *logicFactoryPtr, progressController);

								   std::ranges::move(std::move(images), std::inserter(allFiles, allFiles.end()));
								   RemoveFiles(allFiles, collectionFolder);

								   const auto db          = databaseUser->Database();
								   const auto transaction = db->CreateTransaction();

								   auto ok = RemoveBooksImpl(books, *transaction, std::move(progressItem));
								   ok      = CleanupNavigationItems(*transaction) && ok;
								   ok      = transaction->Commit() && ok;

								   return [callback = std::move(callback), ok](size_t) {
									   callback(ok);
								   };
							   } });
	}
};

CollectionCleaner::CollectionCleaner(
	const std::shared_ptr<const ILogicFactory>&        logicFactory,
	std::shared_ptr<const IDatabaseUser>               databaseUser,
	std::shared_ptr<const ICollectionProvider>         collectionProvider,
	std::shared_ptr<const ILibRateProvider>            libRateProvider,
	std::shared_ptr<IBooksExtractorProgressController> progressController
)
	: m_impl { std::make_unique<Impl>(logicFactory, std::move(databaseUser), std::move(collectionProvider), std::move(libRateProvider), std::move(progressController)) }
{
}

CollectionCleaner::~CollectionCleaner() = default;

void CollectionCleaner::Remove(Books books, Callback callback) const
{
	m_impl->Remove(std::move(books), std::move(callback));
}

void CollectionCleaner::RemovePermanently(Books books, Callback callback) const
{
	m_impl->RemovePermanently(std::move(books), std::move(callback));
}

void CollectionCleaner::Analyze(IAnalyzeObserver& observer) const
{
	m_impl->Analyze(observer);
}

void CollectionCleaner::AnalyzeCancel() const
{
	m_impl->AnalyzeCancel();
}
