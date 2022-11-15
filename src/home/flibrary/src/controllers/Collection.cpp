#include <QCryptographicHash>
#include <QVariant>

#include "util/Settings.h"

#include "Collection.h"

namespace HomeCompa::Flibrary {

namespace {

constexpr auto COLLECTIONS = "Collections";
constexpr auto CURRENT = "current";
constexpr auto DATABASE = "database";
constexpr auto FOLDER = "folder";
constexpr auto NAME = "name";

Collection DeserializeImpl(Settings & settings, QString id)
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
}

QString Collection::GetActive(Settings & settings)
{
	SettingsGroup databaseGroup(settings, COLLECTIONS);
	return settings.Get(CURRENT).toString();
}

void Collection::Serialize(Settings & settings) const
{
	SettingsGroup databaseGroup(settings, COLLECTIONS);
	SettingsGroup idGroup(settings, id);

	settings.Set(NAME, name);
	settings.Set(DATABASE, database);
	settings.Set(FOLDER, folder);
}

Collections Collection::Deserialize(Settings & settings)
{
	Collections collections;
	SettingsGroup settingsGroup(settings, COLLECTIONS);
	std::ranges::transform(settings.GetGroups(), std::back_inserter(collections), [&] (QString groupId)
	{
		return DeserializeImpl(settings, std::move(groupId));
	});

	return collections;
}

void Collection::SetActive(Settings & settings, const QString & id)
{
	SettingsGroup databaseGroup(settings, COLLECTIONS);
	settings.Set(CURRENT, id);
}

void Collection::Remove(Settings & settings, const QString & id)
{
	SettingsGroup databaseGroup(settings, COLLECTIONS);
	settings.Remove(id);
}

QString Collection::GenerateId(const QString & database)
{
	QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
	hash.addData(database.toUtf8());
	return hash.result().toHex();
}

}
