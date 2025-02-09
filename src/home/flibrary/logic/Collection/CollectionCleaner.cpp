#include "CollectionCleaner.h"

#include <QFileInfo>

#include "common/Constant.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IProgressController.h"

#include "Zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

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

}

struct CollectionCleaner::Impl
{
	std::weak_ptr<const ILogicFactory> logicFactory;
	std::shared_ptr<const IDatabaseUser> databaseUser;
	std::shared_ptr<const ICollectionProvider> collectionProvider;
	std::shared_ptr<IBooksExtractorProgressController> progressController;
	bool analyzeCanceled{ false };

	void Analyze(IAnalyzeObserver& observer) const
	{
		databaseUser->Execute({ "Analyze", [&]
		{
			std::function result{[&](size_t) { observer.AnalyzeFinished({}); } };
			return result;
		} });
	}

	void AnalyzeCancel()
	{
		analyzeCanceled = true;
		progressController->Stop();
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
