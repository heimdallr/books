// ReSharper disable once CppUnusedIncludeDirective
#include "CollectionImpl.h"

#include <QFile>

#include "interface/constants/SettingsConstant.h"

#include "util/ISettings.h"
#include "util/hash.h"

#include "log.h"

namespace HomeCompa::Flibrary
{

namespace
{

constexpr auto CURRENT          = "current";
constexpr auto DATABASE         = "database";
constexpr auto DISCARDED_UPDATE = "discardedUpdate";
constexpr auto FOLDER           = "folder";
constexpr auto INPX             = "inpx";
constexpr auto NAME             = "name";
constexpr auto CREATION_MODE    = "creationMode";

} // namespace

CollectionImpl::CollectionImpl(QString name_, QString database, QString folder, QString inpx)
{
	id         = Util::md5(database.toUtf8());
	name       = std::move(name_);
	m_database = std::move(database);
	m_folder   = std::move(folder);
	m_inpx     = std::move(inpx);

	m_database.replace("\\", "/");
	m_folder.replace("\\", "/");
	m_inpx.replace("\\", "/");

	while (m_folder.endsWith("\\"))
		m_folder.resize(m_folder.size() - 1);
}

QString CollectionImpl::GetActive(const ISettings& settings)
{
	SettingsGroup databaseGroup(settings, Constant::Settings::COLLECTIONS);
	return settings.Get(CURRENT, QString {});
}

void CollectionImpl::Serialize(const Collection& collection, ISettings& settings)
{
	SettingsGroup databaseGroup(settings, Constant::Settings::COLLECTIONS);
	SettingsGroup idGroup(settings, collection.id);

	settings.Set(NAME, collection.name);
	settings.Set(DATABASE, collection.m_database);
	settings.Set(FOLDER, collection.m_folder);
	if (!collection.m_inpx.isEmpty())
		settings.Set(INPX, collection.m_inpx);
	settings.Set(DISCARDED_UPDATE, collection.discardedUpdate);
	settings.Set(CREATION_MODE, collection.createCollectionMode);
	settings.Set(Constant::Settings::DESTRUCTIVE_OPERATIONS_ALLOWED_KEY, collection.destructiveOperationsAllowed);
}

Collections CollectionImpl::Deserialize(ISettings& settings)
{
	Collections   collections;
	SettingsGroup settingsGroup(settings, Constant::Settings::COLLECTIONS);
	std::ranges::transform(settings.GetGroups(), std::back_inserter(collections), [&](QString groupId) {
		return Deserialize(settings, std::move(groupId));
	});

	std::erase_if(collections, [&](const auto& item) {
		if (QFile::exists(item->GetDatabase()))
			return false;

		settings.Remove(item->id);
		return true;
	});

	return collections;
}

Collection::Ptr CollectionImpl::Deserialize(const ISettings& settings, QString collectionId)
{
	auto collection = std::make_unique<CollectionImpl>();
	if (collectionId.isEmpty())
		return collection;

	if (!settings.HasGroup(collectionId))
		return collection;

	SettingsGroup idGroup(settings, collectionId);
	collection->id = std::move(collectionId);

	if ((collection->name = settings.Get(NAME, QString {})).isEmpty())
		return collection;

	if ((collection->m_database = settings.Get(DATABASE, QString {})).isEmpty())
		return collection;

	if ((collection->m_folder = settings.Get(FOLDER, QString {})).isEmpty())
		return collection;

	collection->m_inpx = settings.Get(INPX).toString();

	collection->discardedUpdate              = settings.Get(DISCARDED_UPDATE, QString {});
	collection->createCollectionMode         = settings.Get(CREATION_MODE, 0);
	collection->destructiveOperationsAllowed = settings.Get(Constant::Settings::DESTRUCTIVE_OPERATIONS_ALLOWED_KEY, false);

	return collection;
}

void CollectionImpl::SetActive(ISettings& settings, const QString& uid)
{
	SettingsGroup databaseGroup(settings, Constant::Settings::COLLECTIONS);
	if (uid.isEmpty())
	{
		settings.Remove(CURRENT);
		PLOGW << "No active collections";
		return;
	}

	settings.Set(CURRENT, uid);
	PLOGI << "Collection " << uid << " is active now";
}

void CollectionImpl::Remove(ISettings& settings, const QString& uid)
{
	SettingsGroup databaseGroup(settings, Constant::Settings::COLLECTIONS);
	settings.Remove(uid);
	PLOGW << "Collection " << uid << " removed";
}

} // namespace HomeCompa::Flibrary
