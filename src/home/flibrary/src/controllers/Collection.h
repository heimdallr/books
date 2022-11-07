#pragma once

#include <vector>
#include <QString>

namespace HomeCompa {
class Settings;
}

namespace HomeCompa::Flibrary {

struct Collection;
using Collections = std::vector<Collection>;

struct Collection
{
	QString id;
	QString name;
	QString database;
	QString folder;

	Collection() = default;
	Collection(QString name, QString database, QString folder);

	static QString GetActive(Settings & settings);

	void Serialize(Settings & settings) const;
	static Collections Deserialize(Settings & settings);
	static void SetActive(Settings & settings, const QString & id);
	static void Remove(Settings & settings, const QString & id);
};

}
