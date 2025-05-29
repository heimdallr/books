// ReSharper disable once CppUnusedIncludeDirective
#include "CollectionImpl.h"

#include <QFile>
#include <QString> // for plog

#include "interface/constants/SettingsConstant.h"

#include "util/ISettings.h"
#include "util/hash.h"

#include "log.h"

namespace HomeCompa::Flibrary
{

namespace
{

constexpr auto CURRENT = "current";
constexpr auto DATABASE = "database";
constexpr auto DISCARDED_UPDATE = "discardedUpdate";
constexpr auto FOLDER = "folder";
constexpr auto NAME = "name";
constexpr auto CREATION_MODE = "creationMode";

Collection::Ptr DeserializeImpl(const ISettings& settings, QString id)
{
	auto collection = std::make_unique<CollectionImpl>();
	if (id.isEmpty())
		return collection;

	if (!settings.HasGroup(id))
		return collection;

	SettingsGroup idGroup(settings, id);
	collection->id = std::move(id);

	if ((collection->name = settings.Get(NAME, QString {})).isEmpty())
		return collection;

	if ((collection->database = settings.Get(DATABASE, QString {})).isEmpty())
		return collection;

	if ((collection->folder = settings.Get(FOLDER, QString {})).isEmpty())
		return collection;

	collection->discardedUpdate = settings.Get(DISCARDED_UPDATE, QString {});
	collection->createCollectionMode = settings.Get(CREATION_MODE, 0);
	collection->destructiveOperationsAllowed = settings.Get(Constant::Settings::DESTRUCTIVE_OPERATIONS_ALLOWED_KEY, false);

	return collection;
}

} // namespace

CollectionImpl::CollectionImpl(QString name_, QString database_, QString folder_)
{
	id = Util::md5(database_.toUtf8());
	name = std::move(name_);
	database = std::move(database_);
	folder = std::move(folder_);

	database.replace("\\", "/");
	folder.replace("\\", "/");
	while (folder.endsWith("\\"))
		folder.resize(folder.size() - 1);
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
	settings.Set(DATABASE, collection.database);
	settings.Set(FOLDER, collection.folder);
	settings.Set(DISCARDED_UPDATE, collection.discardedUpdate);
	settings.Set(CREATION_MODE, collection.createCollectionMode);
	settings.Set(Constant::Settings::DESTRUCTIVE_OPERATIONS_ALLOWED_KEY, collection.destructiveOperationsAllowed);
}

Collections CollectionImpl::Deserialize(ISettings& settings)
{
	Collections collections;
	SettingsGroup settingsGroup(settings, Constant::Settings::COLLECTIONS);
	std::ranges::transform(settings.GetGroups(), std::back_inserter(collections), [&](QString groupId) { return DeserializeImpl(settings, std::move(groupId)); });

	std::erase_if(collections,
	              [&](const auto& item)
	              {
					  if (QFile::exists(item->database))
						  return false;

					  settings.Remove(item->id);
					  return true;
				  });

	return collections;
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
