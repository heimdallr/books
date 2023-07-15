#include <QCryptographicHash>
// ReSharper disable once CppUnusedIncludeDirective
#include <QString> // for plog

#include <plog/Log.h>

#include "util/ISettings.h"

#include "Collection.h"

namespace HomeCompa::Flibrary {

namespace {

constexpr auto COLLECTIONS = "Collections";
constexpr auto CURRENT = "current";
constexpr auto DATABASE = "database";
constexpr auto DISCARDED_UPDATE = "discardedUpdate";
constexpr auto FOLDER = "folder";
constexpr auto NAME = "name";

Collection DeserializeImpl(ISettings & settings, QString id)
{
	Collection collection;
	if (id.isEmpty())
		return collection;

	if (!settings.HasGroup(id))
		return collection;

	SettingsGroup idGroup(settings, id);

	if ((collection.name = settings.Get(NAME, {}).toString()).isEmpty())
		return collection;

	if ((collection.database = settings.Get(DATABASE, {}).toString()).isEmpty())
		return collection;

	if ((collection.folder = settings.Get(FOLDER, {}).toString()).isEmpty())
		return collection;

	collection.discardedUpdate = settings.Get(DISCARDED_UPDATE, {}).toString();
	collection.id = std::move(id);

	return collection;
}

}

Collection::Collection(QString name_, QString database_, QString folder_)
	: id(GenerateId(database_))
	, name(std::move(name_))
	, database(std::move(database_))
	, folder(std::move(folder_))
{
	database.replace("\\", "/");
	folder.replace("\\", "/");
	while (folder.endsWith("\\"))
		folder.resize(folder.size() - 1);
}

QString Collection::GetActive(ISettings & settings)
{
	SettingsGroup databaseGroup(settings, COLLECTIONS);
	return settings.Get(CURRENT).toString();
}

void Collection::Serialize(ISettings & settings) const
{
	SettingsGroup databaseGroup(settings, COLLECTIONS);
	SettingsGroup idGroup(settings, id);

	settings.Set(NAME, name);
	settings.Set(DATABASE, database);
	settings.Set(FOLDER, folder);
	settings.Set(DISCARDED_UPDATE, discardedUpdate);
}

Collections Collection::Deserialize(ISettings & settings)
{
	Collections collections;
	SettingsGroup settingsGroup(settings, COLLECTIONS);
	std::ranges::transform(settings.GetGroups(), std::back_inserter(collections), [&] (QString groupId)
	{
		return DeserializeImpl(settings, std::move(groupId));
	});

	return collections;
}

void Collection::SetActive(ISettings & settings, const QString & id)
{
	SettingsGroup databaseGroup(settings, COLLECTIONS);
	settings.Set(CURRENT, id);
	PLOGI << "Collection " << id << " is active now";
}

void Collection::Remove(ISettings & settings, const QString & id)
{
	SettingsGroup databaseGroup(settings, COLLECTIONS);
	settings.Remove(id);
	PLOGW << "Collection " << id << " removed";
}

QString Collection::GenerateId(const QString & database)
{
	QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
	hash.addData(database.toUtf8());
	return hash.result().toHex();
}

}
