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

bool RemoveBooksImpl(ICollectionCleaner::Books books, DB::ITransaction& transaction, std::unique_ptr<IProgressController::IProgressItem> progressItem)
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
			auto res = std::accumulate(commands.begin(), commands.end(), true, [&](const bool init, const auto& command)
				{
					command->Bind(0, book.id);
					return command->Execute() && init;
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

}

struct CollectionCleaner::Impl
{
	std::weak_ptr<const ILogicFactory> logicFactory;
	std::shared_ptr<const IDatabaseUser> databaseUser;
	std::shared_ptr<const ICollectionProvider> collectionProvider;
	std::shared_ptr<IBooksExtractorProgressController> progressController;
};

CollectionCleaner::CollectionCleaner(const std::shared_ptr<const ILogicFactory>& logicFactory
	, std::shared_ptr<const IDatabaseUser> databaseUser
	, std::shared_ptr<const ICollectionProvider> collectionProvider
	, std::shared_ptr<IBooksExtractorProgressController> progressController
)
	: m_impl
		{ logicFactory
		, std::move(databaseUser)
		, std::move(collectionProvider)
		, std::move(progressController)
		}
{
}

CollectionCleaner::~CollectionCleaner() = default;

void CollectionCleaner::Remove(Books books, Callback callback)
{
	m_impl->databaseUser->Execute({ "Delete books permanently", [this
		, books = std::move(books)
		, callback = std::move(callback)
		, collectionFolder = m_impl->collectionProvider->GetActiveCollection().folder
	]() mutable
	{
		using AllFilesItem = std::tuple<std::vector<QString>, std::shared_ptr<Zip::ProgressCallback>>;
		std::unordered_map<QString, AllFilesItem> allFiles;
		auto progressItem = m_impl->progressController->Add(100);

		for (auto&& [id, folder, file] : books)
		{
			auto [it, ok] = allFiles.try_emplace(std::move(folder), AllFilesItem{});
			auto& [files, progress] = it->second;
			files.emplace_back(std::move(file));
			if (ok)
				progress = ILogicFactory::Lock(m_impl->logicFactory)->CreateZipProgressCallback(m_impl->progressController);
		}

		const auto getFolderPath = [&](const QString& name)
		{
			return QString("%1/%2").arg(collectionFolder, name);
		};

		static constexpr std::pair<const char*, const char*> imageTypes[]
		{
			{ Global::COVERS, "" },
			{ Global::IMAGES, "/*" },
		};

		decltype(allFiles) images;
		for (const auto& [folder, archiveItem] : allFiles)
		{
			const QFileInfo fileInfo(folder);
			for (const auto& [type, replacedExt] : imageTypes)
			{
				const auto imageFolderName = QString("%1/%2.zip").arg(type, fileInfo.completeBaseName());
				if (QFile::exists(getFolderPath(imageFolderName)))
				{
					auto& [files, progress] = images[imageFolderName];
					std::ranges::transform(std::get<0>(archiveItem), std::back_inserter(files), [=](const QString& file)
					{
						const QFileInfo fileInfo(file);
						return fileInfo.completeBaseName() + replacedExt;
					});
					progress = ILogicFactory::Lock(m_impl->logicFactory)->CreateZipProgressCallback(m_impl->progressController);
				}
			}
		}

		std::ranges::move(std::move(images), std::inserter(allFiles, allFiles.end()));

		for (auto&& [folder, archiveItem] : allFiles)
		{
			const auto archive = getFolderPath(folder);
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

		const auto db = m_impl->databaseUser->Database();
		const auto transaction = db->CreateTransaction();

		auto ok = RemoveBooksImpl(std::move(books), *transaction, std::move(progressItem));
		ok = CleanupNavigationItems(*transaction) && ok;
		ok = transaction->Commit() && ok;

		return [callback = std::move(callback), ok](size_t) { callback(ok); };
	} });
}
