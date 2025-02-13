#include "CollectionUpdateChecker.h"

#include <QCryptographicHash>

#include <QFileInfo>

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/logic/ICollectionController.h"
#include "interface/logic/IDatabaseUser.h"

#include "log.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

QString GetFileHash(const std::set<QString>& fileNames)
{
	QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
	for (const auto& fileName : fileNames)
	{
		QFile file(fileName);
		if (!file.open(QIODevice::ReadOnly))
			return {};

		constexpr auto size = 1024ll * 32;
		const auto buf = std::make_unique<char[]>(size);

		while (const auto readSize = file.read(buf.get(), size))
			hash.addData(QByteArrayView(buf.get(), static_cast<int>(readSize)));
	}

	return hash.result().toHex();
}

QStringList& PrepareFolders(QStringList& folders)
{
	std::ranges::transform(folders, folders.begin(), [](const auto& item) { return QFileInfo(item).completeBaseName().toLower(); });
	std::ranges::sort(folders);
	return folders;
}

QStringList GetDbFolders(DB::IDatabase& db)
{
	QStringList folders;
	const auto query = db.CreateQuery("select FolderTitle from Folders");
	for (query->Execute(); !query->Eof(); query->Next())
		folders << query->Get<const char*>(0);

	return PrepareFolders(folders);
}

QStringList GetInpxFolders(const ICollectionController& collectionController, Collection& updatedCollection)
{
	if (!collectionController.ActiveCollectionExists())
		return {};

	const auto& collection = collectionController.GetActiveCollection();

	updatedCollection = collection;
	const auto inpxFileNames = collectionController.GetInpxFiles(collection.folder);
	if (inpxFileNames.empty())
		return {};

	if (updatedCollection.discardedUpdate = GetFileHash(inpxFileNames); updatedCollection.discardedUpdate == collection.discardedUpdate)
		return {};

	QStringList folders;

	for (const auto& inpxFileName : inpxFileNames)
	{
		assert(QFile::exists(inpxFileName));

		const Zip zip(inpxFileName);
		folders << zip.GetFileNameList();
	}

	if (auto [begin, end] = std::ranges::remove_if(folders, [](const auto& item) { return QFileInfo(item).suffix().toLower() != "inp"; }); begin != end)
		folders.erase(begin, end);

	return PrepareFolders(folders);
}

} // namespace

struct CollectionUpdateChecker::Impl
{
	std::shared_ptr<ICollectionController> collectionController;
	std::shared_ptr<const IDatabaseUser> databaseUser;

	Impl(std::shared_ptr<ICollectionController> collectionController, std::shared_ptr<const IDatabaseUser> databaseUser)
		: collectionController(std::move(collectionController))
		, databaseUser(std::move(databaseUser))
	{
	}
};

CollectionUpdateChecker::CollectionUpdateChecker(std::shared_ptr<ICollectionController> collectionController, std::shared_ptr<IDatabaseUser> databaseUser)
	: m_impl(std::move(collectionController), std::move(databaseUser))
{
	PLOGV << "CollectionUpdateChecker created";
}

CollectionUpdateChecker::~CollectionUpdateChecker()
{
	PLOGV << "CollectionUpdateChecker destroyed";
}

void CollectionUpdateChecker::CheckForUpdate(Callback callback) const
{
	if (!(m_impl->collectionController->ActiveCollectionExists() && m_impl->collectionController->GetActiveCollection().updatable))
		return callback(false);

	auto db = m_impl->databaseUser->Database();
	m_impl->databaseUser->Execute({ "Check for collection index updated",
	                                [&, db = std::move(db), callback = std::move(callback)]() mutable
	                                {
										Collection updatedCollection;
										QStringList addedFolders;
										auto getResult = [&]()
										{
											return [collectionController = m_impl->collectionController,
			                                        updatedCollection = std::move(updatedCollection),
			                                        addedFolders = std::move(addedFolders),
			                                        callback = std::move(callback)](size_t) mutable
											{
												if (addedFolders.isEmpty())
													return callback(false);

												collectionController->OnInpxUpdateFound(updatedCollection);
												callback(true);
											};
										};

										const auto inpxFolders = GetInpxFolders(*m_impl->collectionController, updatedCollection);
										if (inpxFolders.isEmpty())
											return getResult();

										const auto dbFolders = GetDbFolders(*db);
										std::ranges::set_difference(inpxFolders, dbFolders, std::back_inserter(addedFolders));

										return getResult();
									} },
	                              100);
}
