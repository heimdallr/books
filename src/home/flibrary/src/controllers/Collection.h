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

	void SetActive(Settings & settings) const;
	static QString GetActive(Settings & settings);

	void Serialize(Settings & settings) const;
//	static Collection Deserialize(Settings & settings, QString id);
	static Collections Deserialize(Settings & settings);
};

}
