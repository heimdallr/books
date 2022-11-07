#include <QCryptographicHash>
#include <QVariant>

#include "util/Settings.h"

#include "Collection.h"

namespace HomeCompa::Flibrary {

namespace {

constexpr auto CURRENT = "current";
constexpr auto DATABASE = "database";
constexpr auto FOLDER = "folder";
constexpr auto NAME = "name";

QString GenerateId(const QString & database)
{
	QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
	hash.addData(database.toUtf8());
	return hash.result().toHex();
}

}

Collection::Collection(QString name_, QString database_, QString folder_)
	: id(GenerateId(database_))
	, name(std::move(name_))
	, database(std::move(database_))
	, folder(std::move(folder_))
{
}

void Collection::SetActive(Settings & settings) const
{
	SettingsGroup databaseGroup(settings, DATABASE);
	settings.Set(CURRENT, id);
}

QString Collection::GetActive(Settings & settings)
{
	SettingsGroup databaseGroup(settings, DATABASE);
	return settings.Get(CURRENT).toString();
}

void Collection::Serialize(Settings & settings) const
{
	SettingsGroup databaseGroup(settings, DATABASE);
	SettingsGroup idGroup(settings, id);

	settings.Set(NAME, name);
	settings.Set(DATABASE, database);
	settings.Set(FOLDER, folder);
}

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

Collections Collection::Deserialize(Settings & settings)
{
	Collections collections;
	SettingsGroup settingsGroup(settings, DATABASE);
	std::ranges::transform(settings.GetGroups(), std::back_inserter(collections), [&] (QString id)
	{
		return DeserializeImpl(settings, std::move(id));
	});

	return collections;
}

}
