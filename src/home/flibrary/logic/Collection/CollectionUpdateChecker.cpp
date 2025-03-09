#include "CollectionUpdateChecker.h"

#include <QCryptographicHash>

#include <QFileInfo>

#include "database/interface/IDatabase.h"

#include "interface/logic/ICollectionController.h"
#include "interface/logic/IDatabaseUser.h"

#include "inpx/src/util/inpx.h"

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
	if (!m_impl->collectionController->ActiveCollectionExists())
		return callback(false);

	auto db = m_impl->databaseUser->Database();
	m_impl->databaseUser->Execute({ "Check for collection index updated",
	                                [&, db = std::move(db), callback = std::move(callback)]() mutable
	                                {
										std::function<void(size_t)> result;

										const auto& collection = m_impl->collectionController->GetActiveCollection();
										const auto collectionFolder = collection.folder;
										const auto inpxFiles = m_impl->collectionController->GetInpxFiles(collectionFolder);

										Collection updatedCollection = collection;
										if (updatedCollection.discardedUpdate = GetFileHash(inpxFiles); updatedCollection.discardedUpdate == collection.discardedUpdate)
										{
											result = [callback = std::move(callback)](size_t) { callback(false); };
											return result;
										}

										const auto checkResult = Inpx::Parser::CheckForUpdate(collectionFolder.toStdWString(), *db);
										result =
											[checkResult, collectionController = m_impl->collectionController, updatedCollection = std::move(updatedCollection), callback = std::move(callback)](size_t) mutable
										{
											if (!checkResult)
												return callback(false);

											collectionController->OnInpxUpdateChecked(updatedCollection);
											callback(true);
										};

										return result;
									} },
	                              100);
}
