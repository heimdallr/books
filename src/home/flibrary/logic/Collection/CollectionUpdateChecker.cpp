#include "CollectionUpdateChecker.h"

#include <QCryptographicHash>

#include <QFileInfo>

#include "database/interface/IDatabase.h"

#include "inpx/inpx.h"

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
		const auto     buf  = std::make_unique<char[]>(size);

		while (const auto readSize = file.read(buf.get(), size))
			hash.addData(QByteArrayView(buf.get(), static_cast<int>(readSize)));
	}

	return hash.result().toHex();
}

} // namespace

struct CollectionUpdateChecker::Impl
{
	std::shared_ptr<const ICollectionProvider> collectionProvider;
	std::shared_ptr<const IDatabaseUser>       databaseUser;

	Impl(std::shared_ptr<const ICollectionProvider> collectionProvider, std::shared_ptr<const IDatabaseUser> databaseUser)
		: collectionProvider { std::move(collectionProvider) }
		, databaseUser { std::move(databaseUser) }
	{
	}
};

CollectionUpdateChecker::CollectionUpdateChecker(std::shared_ptr<const ICollectionProvider> collectionProvider, std::shared_ptr<const IDatabaseUser> databaseUser)
	: m_impl(std::move(collectionProvider), std::move(databaseUser))
{
	PLOGV << "CollectionUpdateChecker created";
}

CollectionUpdateChecker::~CollectionUpdateChecker()
{
	PLOGV << "CollectionUpdateChecker destroyed";
}

void CollectionUpdateChecker::CheckForUpdate(Callback callback) const
{
	if (!m_impl->collectionProvider->ActiveCollectionExists())
		return callback(false, Collection {});

	auto db = m_impl->databaseUser->Database();
	m_impl->databaseUser->Execute(
		{ "Check for collection index updated",
	      [&, db = std::move(db), callback = std::move(callback)]() mutable {
			  std::function<void(size_t)> result;

			  const auto& collection       = m_impl->collectionProvider->GetActiveCollection();
			  const auto  collectionFolder = collection.GetFolder();
			  const auto  explicitInpx     = collection.GetInpx();

			  std::set<QString> inpxFiles;
			  if (!explicitInpx.isEmpty())
				  inpxFiles.insert(explicitInpx);
			  else
				  inpxFiles = m_impl->collectionProvider->GetInpxFiles(collectionFolder);

			  Collection updatedCollection = collection;
			  if (updatedCollection.discardedUpdate = GetFileHash(inpxFiles); updatedCollection.discardedUpdate == collection.discardedUpdate)
			  {
				  result = [&updatedCollection, callback = std::move(callback)](size_t) {
					  callback(false, updatedCollection);
				  };
				  return result;
			  }

			  auto [_, ini]          = m_impl->collectionProvider->GetIniMap(collection.GetDatabase(), collection.GetFolder(), explicitInpx, false);
			  const auto checkResult = Inpx::Parser::CheckForUpdate(std::move(ini), *db);
			  result                 = [checkResult, updatedCollection = std::move(updatedCollection), callback = std::move(callback)](size_t) mutable {
                  callback(checkResult, updatedCollection);
			  };

			  return result;
		  } },
		100
	);
}
