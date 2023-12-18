#include "CollectionUpdateChecker.h"

#include <QCryptographicHash>
#include <QFileInfo>

#include <plog/Log.h>

#include "interface/logic/ICollectionController.h"

#include "shared/DatabaseUser.h"

#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

QString GetFileHash(const QString & fileName)
{
	QFile file(fileName);
	file.open(QIODevice::ReadOnly);
	constexpr auto size = 1024ll * 32;
	const std::unique_ptr<char[]> buf(new char[size]);

	QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);

	while (const auto readSize = file.read(buf.get(), size))
		hash.addData(QByteArrayView(buf.get(), static_cast<int>(readSize)));

	return hash.result().toHex();
}

QStringList & PrepareFolders(QStringList & folders)
{
	std::ranges::transform(folders, folders.begin(), [] (const auto & item) { return QFileInfo(item).completeBaseName().toLower(); });
	std::ranges::sort(folders);
	return folders;
}

QStringList GetDbFolders(const DatabaseUser & databaseUser)
{
	QStringList folders;
	const auto db = databaseUser.Database();
	const auto query = db->CreateQuery("select distinct Folder from Books");
	for (query->Execute(); !query->Eof(); query->Next())
		folders << query->Get<const char *>(0);

	return PrepareFolders(folders);
}

QStringList GetInpxFolders(const ICollectionController & collectionController, Collection & updatedCollection)
{
	const auto collection = collectionController.GetActiveCollection();
	if (!collection)
		return {};

	updatedCollection = *collection;
	const auto inpxFileName = collectionController.GetInpx(collection->folder);
	assert(!inpxFileName.isEmpty());

	if (updatedCollection.discardedUpdate = GetFileHash(inpxFileName); updatedCollection.discardedUpdate == collection->discardedUpdate)
		return {};

	const Zip zip(inpxFileName);
	auto folders = zip.GetFileNameList();
	auto [begin, end] = std::ranges::remove_if(folders, [] (const auto & item)
	{
		return QFileInfo(item).suffix().toLower() != "inp";
	});
	folders.erase(begin, end);

	return PrepareFolders(folders);
}

}

struct CollectionUpdateChecker::Impl
{
	std::shared_ptr<ICollectionController> collectionController;
	std::shared_ptr<const DatabaseUser> databaseUser;

	Impl(std::shared_ptr<ICollectionController> collectionController
		, std::shared_ptr<const DatabaseUser> databaseUser
	)
		: collectionController(std::move(collectionController))
		, databaseUser(std::move(databaseUser))
	{
	}
};

CollectionUpdateChecker::CollectionUpdateChecker(std::shared_ptr<ICollectionController> collectionController
	, std::shared_ptr<DatabaseUser> databaseUser
)
	: m_impl(std::move(collectionController)
		, std::move(databaseUser)
	)
{
	PLOGD << "CollectionUpdateChecker created";
}

CollectionUpdateChecker::~CollectionUpdateChecker()
{
	PLOGD << "CollectionUpdateChecker destroyed";
}

void CollectionUpdateChecker::CheckForUpdate(Callback callback) const
{
	m_impl->databaseUser->Execute({ "Check for collection index updated", [&, callback = std::move(callback)] () mutable
	{
		const auto collection = m_impl->collectionController->GetActiveCollection();
		Collection updatedCollection;
		QStringList addedFolders;
		auto getResult = [&]()
		{
			return [collectionController = m_impl->collectionController
				, updatedCollection = std::move(updatedCollection)
				, addedFolders = std::move(addedFolders)
				, callback = std::move(callback)
			] (size_t) mutable
			{
				if (!addedFolders.isEmpty())
					collectionController->OnInpxUpdateFound(updatedCollection);

				callback(!addedFolders.isEmpty());
			};
		};

		if (!collection)
			return getResult();

		const auto inpxFolders = GetInpxFolders(*m_impl->collectionController, updatedCollection);
		if (inpxFolders.isEmpty())
			return getResult();

		const auto dbFolders = GetDbFolders(*m_impl->databaseUser);
		std::ranges::set_difference(inpxFolders, dbFolders, std::back_inserter(addedFolders));

		return getResult();
	} }, 100);
}
