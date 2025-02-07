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

using namespace HomeCompa::Flibrary;

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
			auto&& [files, progressCallback] = archiveItem;
			Zip zip(getFolderPath(folder), Zip::Format::Auto, true, std::move(progressCallback));
			zip.Remove(files);
		}

		const auto db = m_impl->databaseUser->Database();
		const auto transaction = db->CreateTransaction();
		const auto commandBooks = transaction->CreateCommand("delete from Books where BookID = ?");
		const auto commandAuthorList = transaction->CreateCommand("delete from Author_List where BookID = ?");
		const auto commandKeywordList = transaction->CreateCommand("delete from Keyword_List where BookID = ?");

		auto ok = std::accumulate(books.cbegin(), books.cend(), true,[&
			, progressItem = std::move(progressItem)
			, total = books.size()
			, currentPct = int64_t{ 0 }
			, lastPct = int64_t{ 0 }
			, n = size_t{ 0 }
		](const bool init, const auto& book) mutable
		{
			commandBooks->Bind(0, book.id);
			commandAuthorList->Bind(0, book.id);
			commandKeywordList->Bind(0, book.id);

			auto res = commandBooks->Execute();
			res = commandAuthorList->Execute() || res;
			res = commandKeywordList->Execute() || res;

			currentPct = 100 * n / total;
			progressItem->Increment(currentPct - lastPct);
			lastPct = currentPct;

			return res && init;
		});
		ok = transaction->Commit() && ok;

		return [callback = std::move(callback), ok](size_t) { callback(ok); };
	} });
}
