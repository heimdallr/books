#pragma once

#include <QString>

namespace HomeCompa {
class Settings;
}

namespace HomeCompa::Flibrary {

struct Collection
{
	QString id;
	QString name;
	QString database;
	QString folder;

	Collection() = default;
	Collection(QString name, QString database, QString folder);

	void SetActive(Settings & settings) const;

	void Serialize(Settings & settings) const;
	static Collection Deserialize(Settings & settings, QString id);
};

}
