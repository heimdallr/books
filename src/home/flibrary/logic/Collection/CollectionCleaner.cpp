#include "CollectionCleaner.h"

#include <ranges>

#include <QFileInfo>

#include "common/Constant.h"

#include "fnd/algorithm.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IProgressController.h"

#include "Zip.h"
#include "database/interface/IQuery.h"
#include "inpx/src/util/constant.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

struct AnalyzedBook
{
	QString folder;
	QString file;
	QString lang;
	bool deleted;
	QString date;
	QString title;
	std::set<QString> genres;
	std::set<long long> authors;
};
using AnalyzedBooks = std::unordered_map<long long, AnalyzedBook>;

constexpr auto SELECT_ANALYZED_BOOKS_QUERY = R"(select b.BookID, f.FolderTitle, b.FileName || b.Ext, b.Lang, coalesce(bu.IsDeleted, b.IsDeleted, 0), b.UpdateDate, b.Title, %1, %3 
	from Books b 
	join Folders f on f.FolderID = b.FolderID %2 %4 
	left join Books_User bu on bu.BookID = b.BookID)";

constexpr auto EMPTY_FIELD = "42";
constexpr auto GENRE_JOIN = "\n	join Genre_List gl on gl.BookID = b.BookID";
constexpr auto GENRE_FIELD = "gl.GenreCode";
constexpr auto AUTHOR_JOIN = "\n	join Author_List al on al.BookID = b.BookID";
constexpr auto AUTHOR_FIELD = "al.AuthorID";

template<typename Value, size_t ArraySize>
std::vector<std::unique_ptr<DB::ICommand>> CreateCommands(Value (&commandTexts)[ArraySize], DB::ITransaction& transaction)
{
	std::vector<std::unique_ptr<DB::ICommand>> commands;
	commands.reserve(ArraySize);
	std::ranges::transform(commandTexts, std::back_inserter(commands), [&](const char* text) { return transaction.CreateCommand(text); });
	return commands;
}

bool RemoveBooksImpl(const ICollectionCleaner::Books& books, DB::ITransaction& transaction, std::unique_ptr<IProgressController::IProgressItem> progressItem)
{
	constexpr const char* commandTexts[]
	{
		"delete from Books where BookID = ?",
		"delete from Author_List where BookID = ?",
		"delete from Keyword_List where BookID = ?",
		"delete from Genre_List where BookID = ?",
	};
	const auto commands = CreateCommands(commandTexts, transaction);

	return std::accumulate(books.cbegin(), books.cend(), true, [&
		, progressItem = std::move(progressItem)
		, total = books.size()
		, currentPct = int64_t{ 0 }
		, lastPct = int64_t{ 0 }
		, n = size_t{ 0 }
	](const bool init, const auto& book) mutable
		{
			auto res = std::accumulate(commands.begin(), commands.end(), true, [&](const bool initAccumulate, const auto& command)
				{
					command->Bind(0, book.id);
					return command->Execute() && initAccumulate;
				});

			currentPct = 100 * n / total;
			progressItem->Increment(currentPct - lastPct);
			lastPct = currentPct;

			return res && init;
		});
}

bool CleanupNavigationItems(DB::ITransaction& transaction)
{
	constexpr const char* commandTexts[]
	{
		"delete from Series where not exists(select 42 from Books where Books.SeriesID = Series.SeriesID)",
		"delete from Authors where not exists(select 42 from Author_List where Author_List.AuthorID = Authors.AuthorID)",
		"delete from Keywords where not exists(select 42 from Keyword_List where Keyword_List.KeywordID = Keywords.KeywordID)",
	};
	const auto commands = CreateCommands(commandTexts, transaction);
	return std::accumulate(commands.begin(), commands.end(), true, [&](const bool init, const auto& command) { return command->Execute() && init; });
}
using AllFilesItem = std::tuple<std::vector<QString>, std::shared_ptr<Zip::ProgressCallback>>;
using AllFiles = std::unordered_map<QString, AllFilesItem>;

AllFiles CollectBookFiles(ICollectionCleaner::Books& books
	, const ILogicFactory& logicFactory
	, const std::shared_ptr<IProgressController>& progressController
)
{
	AllFiles result;
	for (auto&& [id, folder, file] : books)
	{
		auto [it, ok] = result.try_emplace(std::move(folder), AllFilesItem{});
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
};

AllFiles CollectImageFiles(const AllFiles& bookFiles
	, const QString& collectionFolder
	, const ILogicFactory& logicFactory
	, const std::shared_ptr<IProgressController>& progressController
)
{
	static constexpr std::pair<const char*, const char*> imageTypes[]
	{
		{ Global::COVERS, "" },
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
				std::ranges::transform(std::get<0>(archiveItem), std::back_inserter(files), [=](const QString& file)
					{
						return QFileInfo(file).completeBaseName() + replacedExt;
					});
				progress = logicFactory.CreateZipProgressCallback(progressController);
			}
		}
	}

	return result;
}

void RemoveFiles(AllFiles& allFiles
	, const QString& collectionFolder
)
{
	for (auto&& [folder, archiveItem] : allFiles)
	{
		const auto archive = GetFolderPath(collectionFolder, folder);
		if ([&]
			{
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

std::unordered_set<QString> GetAddDateGenres(DB::IDatabase& db)
{
	std::unordered_set<QString> result;
	std::unordered_map<QString, std::vector<QString>> genres;
	std::vector<QString> stack;

	const auto dateAddedCode = QString::fromStdWString(DATE_ADDED_CODE);

	const auto query = db.CreateQuery("select ParentCode, GenreCode, FB2Code from Genres");
	for (query->Execute(); !query->Eof(); query->Next())
	{
		genres[query->Get<const char*>(0)].emplace_back(query->Get<const char*>(1));
		if (query->Get<const char*>(2) == dateAddedCode)
			stack.emplace_back(query->Get<const char*>(1));
	}

	while (!stack.empty())
	{
		auto item = std::move(stack.back());
		stack.pop_back();
		const auto it = genres.find(item);
		if (it == genres.end())
			continue;

		std::ranges::copy(it->second, std::inserter(result, result.end()));
		stack.reserve(stack.size() + it->second.size());
		std::ranges::move(std::move(it->second), std::back_inserter(stack));
	}

	return result;
}

AnalyzedBooks GetAnalysedBooks(DB::IDatabase& db
	, const ICollectionCleaner::IAnalyzeObserver& observer
	, const bool hasGenres
	, const std::atomic_bool& analyzeCanceled
)
{
	const auto addDateGenres = GetAddDateGenres(db);

	const auto query = db.CreateQuery(QString(SELECT_ANALYZED_BOOKS_QUERY)
		.arg(hasGenres ? GENRE_FIELD : EMPTY_FIELD)
		.arg(hasGenres ? GENRE_JOIN : "")
		.arg(observer.NeedDeleteDuplicates() ? AUTHOR_FIELD : EMPTY_FIELD)
		.arg(observer.NeedDeleteDuplicates() ? AUTHOR_JOIN : "")
		.toStdString());

	AnalyzedBooks analyzedBooks;
	for (query->Execute(); !query->Eof(); query->Next())
	{
		auto [it, inserted] = analyzedBooks.try_emplace(query->Get<long long>(0), AnalyzedBook{});
		if (inserted)
		{
			it->second.folder = query->Get<const char*>(1);
			it->second.file = query->Get<const char*>(2);
			it->second.lang = query->Get<const char*>(3);
			it->second.deleted = query->Get<int>(4);
			it->second.date = query->Get<const char*>(5);
			it->second.title = QString(query->Get<const char*>(6)).toLower();
		}

		if (QString genre = query->Get<const char*>(7); !addDateGenres.contains(genre))
			it->second.genres.emplace(std::move(genre));
		it->second.authors.emplace(query->Get<long long>(8));
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
			duplicates[book.title].emplace(book.date, id);

	for (auto& dup : duplicates | std::views::values | std::views::filter([](const auto& item) { return item.size() > 1; }))
	{
		while (!dup.empty())
		{
			const auto id = dup.begin()->second;
			dup.erase(dup.begin());
			const auto itOrigin = analysedBooks.find(id);
			assert(itOrigin != analysedBooks.end());

			for (auto iit = dup.begin(); iit != dup.end(); )
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

}

struct CollectionCleaner::Impl
{
	std::weak_ptr<const ILogicFactory> logicFactory;
	std::shared_ptr<const IDatabaseUser> databaseUser;
	std::shared_ptr<const ICollectionProvider> collectionProvider;
	std::shared_ptr<IBooksExtractorProgressController> progressController;
	std::atomic_bool analyzeCanceled{ false };

	void Analyze(IAnalyzeObserver& observer) const
	{
		databaseUser->Execute({ "Analyze", [&]
		{
			std::function result{[&](size_t) { observer.AnalyzeFinished({}); } };
			auto genres = observer.GetGenres();
			auto languages = observer.GetLanguages();
			if (genres.isEmpty() && languages.isEmpty() && !observer.NeedDeleteDuplicates() && !observer.NeedDeleteMarkedAsDeleted())
				return result;

			const auto db = databaseUser->Database();
			auto analysedBooks = GetAnalysedBooks(*db, observer, !genres.isEmpty(), analyzeCanceled);

			std::unordered_set<long long> toDelete;

			const auto addToDelete = [&](const auto filter)
			{
				std::ranges::transform(analysedBooks | std::views::filter(filter), std::inserter(toDelete, toDelete.end()), [](const auto& item) { return item.first; });
			};

			if (observer.NeedDeleteMarkedAsDeleted())
				addToDelete([](const auto& item) { return item.second.deleted; });

			if (!languages.isEmpty())
				addToDelete([languages = std::unordered_set(std::make_move_iterator(languages.begin()), std::make_move_iterator(languages.end()))](const auto& item) { return languages.contains(item.second.lang); });

			if (!genres.isEmpty())
			{
				const auto indexedGenres = std::set(std::make_move_iterator(genres.begin()), std::make_move_iterator(genres.end()));
				switch(observer.GetCleanGenreMode())
				{
					case CleanGenreMode::Full:
						addToDelete([&](const auto& item) { return std::ranges::includes(item.second.genres, indexedGenres); });
						break;
					case CleanGenreMode::Partial:
						addToDelete([&](const auto& item) { return Util::Intersect(item.second.genres, indexedGenres); });
						break;
					default:  // NOLINT(clang-diagnostic-covered-switch-default)
						assert(false && "unexpected mode");
						break;
				}
			}

			if (observer.NeedDeleteDuplicates())
				RemoveDuplicates(analysedBooks, toDelete);
			
			Books books;
			books.reserve(toDelete.size());
			std::ranges::transform(toDelete, std::back_inserter(books), [&](const long long id)
			{
				const auto it = analysedBooks.find(id);
				assert(it != analysedBooks.end());
				return Book{id, std::move(it->second.folder), std::move(it->second.file)};
			});

			result = [&, books = std::move(books)](size_t) mutable { observer.AnalyzeFinished(std::move(books)); };

			return result;
		} });
	}

	void AnalyzeCancel()
	{
		analyzeCanceled = true;
	}

	void Remove(Books books, Callback callback) const
	{
		databaseUser->Execute({ "Delete books permanently", [this
			, books = std::move(books)
			, callback = std::move(callback)
			, collectionFolder = collectionProvider->GetActiveCollection().folder
		]() mutable
		{
			auto progressItem = progressController->Add(100);

			auto logicFactoryPtr = ILogicFactory::Lock(logicFactory);
			auto allFiles = CollectBookFiles(books, *logicFactoryPtr, progressController);
			auto images = CollectImageFiles(allFiles, collectionFolder, *logicFactoryPtr, progressController);

			std::ranges::move(std::move(images), std::inserter(allFiles, allFiles.end()));
			RemoveFiles(allFiles, collectionFolder);

			const auto db = databaseUser->Database();
			const auto transaction = db->CreateTransaction();

			auto ok = RemoveBooksImpl(books, *transaction, std::move(progressItem));
			ok = CleanupNavigationItems(*transaction) && ok;
			ok = transaction->Commit() && ok;

			return [callback = std::move(callback), ok](size_t) { callback(ok); };
		} });
	}
};

CollectionCleaner::CollectionCleaner(const std::shared_ptr<const ILogicFactory>& logicFactory
	, std::shared_ptr<const IDatabaseUser> databaseUser
	, std::shared_ptr<const ICollectionProvider> collectionProvider
	, std::shared_ptr<IBooksExtractorProgressController> progressController
)
	: m_impl{ std::make_unique<Impl>
		( logicFactory
		, std::move(databaseUser)
		, std::move(collectionProvider)
		, std::move(progressController)
		)
	}
{
}

CollectionCleaner::~CollectionCleaner() = default;

void CollectionCleaner::Remove(Books books, Callback callback) const
{
	m_impl->Remove(std::move(books), std::move(callback));
}

void CollectionCleaner::Analyze(IAnalyzeObserver& observer) const
{
	m_impl->Analyze(observer);
}

void CollectionCleaner::AnalyzeCancel() const
{
	m_impl->AnalyzeCancel();
}
